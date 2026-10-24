import { describe, expect, it } from '@jest/globals';
import {
  buildSectionSource,
  createSectionSourceState,
  rowsOf,
  type FlatRow,
} from '../virtualizer/sectionSource';
import {
  footerRowKey,
  headerRowKey,
  itemRowKey,
} from '../virtualizer/sectionKeys';
import type { ListStore } from '../virtualizer/nativeStore';
import type { SectionListData } from '../types';
import { createFakeStoreModule } from '../__fixtures__/shadowListStore';

interface Item {
  id: string;
}

type Section = SectionListData<Item, { title: string }>;
type Row = FlatRow<Item, { title: string }>;

/*
 * The flatten SectionList ran in JS before the rows moved to a source with native keys: one
 * pass over every item, reusing a row by id when all its fields match. Its keys now follow the
 * core's scheme, see sectionKeys.ts.
 */
function referenceFlatten(
  sections: ReadonlyArray<Section>,
  hasHeader: boolean,
  hasFooter: boolean,
  previousRows: Map<string, Row>
): { rows: Row[]; stickyIndices: number[]; nextRows: Map<string, Row> } {
  const rows: Row[] = [];
  const stickyIndices: number[] = [];
  const nextRows = new Map<string, Row>();
  const push = (row: Row) => {
    const previous = previousRows.get(row.id);
    const same =
      previous !== undefined &&
      previous.type === row.type &&
      previous.sectionIndex === row.sectionIndex &&
      previous.section === row.section &&
      previous.item === row.item &&
      previous.itemIndex === row.itemIndex &&
      previous.rowKey === row.rowKey &&
      previous.isLastInSection === row.isLastInSection &&
      previous.isSectionBoundary === row.isSectionBoundary;
    const final = same ? previous : row;
    nextRows.set(final.id, final);
    rows.push(final);
  };
  sections.forEach((section, sectionIndex) => {
    const sectionKey = section.key ?? `section-${sectionIndex}`;
    if (hasHeader) {
      stickyIndices.push(rows.length);
      push({
        id: headerRowKey(sectionKey),
        type: 'sectionHeader',
        section,
        sectionIndex,
      });
    }
    const last = section.data.length - 1;
    section.data.forEach((item, itemIndex) => {
      const isLastInSection = itemIndex === last;
      push({
        id: itemRowKey(sectionKey, item.id),
        type: 'item',
        sectionIndex,
        item,
        itemIndex,
        rowKey: item.id,
        isLastInSection,
        isSectionBoundary:
          isLastInSection && !hasFooter && sectionIndex < sections.length - 1,
      });
    });
    if (hasFooter) {
      push({
        id: footerRowKey(sectionKey),
        type: 'sectionFooter',
        section,
        sectionIndex,
        isSectionBoundary: sectionIndex < sections.length - 1,
      });
    }
  });
  return { rows, stickyIndices, nextRows };
}

function fakeStore(): {
  store: ListStore;
  module: ReturnType<typeof createFakeStoreModule>;
} {
  const module = createFakeStoreModule();
  const handle = module.createStore();
  return { store: { module, handle, id: handle.id, version: 0 }, module };
}

describe('section sources', () => {
  for (const withStore of [false, true]) {
    it(`match the reference flatten and reuse the same rows ${withStore ? 'with' : 'without'} the native store`, () => {
      let seed = 11;
      const random = () => {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        return seed / 2147483648;
      };
      let nextId = 0;
      const item = (): Item => ({ id: `i${nextId++}` });
      let nextSection = 0;
      const section = (count: number): Section => ({
        key: `s${nextSection++}`,
        title: 'x',
        data: Array.from({ length: count }, item),
      });

      for (const [hasHeader, hasFooter] of [
        [true, false],
        [false, true],
        [true, true],
        [false, false],
      ] as const) {
        const { store, module } = fakeStore();
        const state = createSectionSourceState<Item, { title: string }>();
        let sections: Section[] = [section(3), section(0), section(4)];
        let previousRows = new Map<string, Row>();
        let previousFlat: Row[] = [];
        let previousReference: Row[] = [];

        for (let step = 0; step < 400; step++) {
          const next = [...sections];
          const change = Math.floor(random() * 7);
          const at = Math.floor(random() * next.length);
          const target = next[at];
          if (change === 0 && target) {
            next[at] = { ...target, data: [item(), ...target.data] };
          } else if (change === 1 && target) {
            next[at] = { ...target, data: target.data.slice(1) };
          } else if (change === 2) {
            next.splice(at, 0, section(1 + (step % 3)));
          } else if (change === 3 && next.length > 1) {
            next.splice(at, 1);
          } else if (change === 4 && target && target.data.length > 0) {
            // Edited in place, same section and array objects.
            (target.data as Item[])[0] = item();
          } else if (change === 5 && target) {
            next[at] = { ...target, data: [...target.data, item()] };
          }
          if (next.length > 8) next.splice(0, 4);

          const source = buildSectionSource(
            state,
            next,
            undefined,
            { hasHeader, hasFooter, stickyEnabled: true },
            withStore ? store : null
          );
          const reference = referenceFlatten(
            next,
            hasHeader,
            hasFooter,
            previousRows
          );
          const rows = rowsOf(source);
          expect(rows).toEqual(reference.rows);
          expect(rows.map((_, row) => source.keyAt(row))).toEqual(
            reference.rows.map((row) => row.id)
          );
          if (withStore) {
            const written = module.stores[0]!.versions.get(source.keysVersion)!;
            expect(written.keys).toEqual(reference.rows.map((row) => row.id));
            expect(written.sticky).toEqual(
              hasHeader ? reference.stickyIndices : []
            );
            expect(source.propKeys).toEqual([]);
            expect(source.stickyIndices).toBeNull();
          } else {
            expect(source.propKeys).toEqual(
              reference.rows.map((row) => row.id)
            );
            expect(source.stickyIndices ?? []).toEqual(
              hasHeader ? reference.stickyIndices : []
            );
          }
          // A row keeps its object exactly when the reference kept it.
          const kept = (list: Row[], before: Row[]) => {
            const byId = new Map(before.map((row) => [row.id, row]));
            return list.map((row) => byId.get(row.id) === row);
          };
          expect(kept(rows, previousFlat)).toEqual(
            kept(reference.rows, previousReference)
          );
          // Every row is found by its key, near a hint or not.
          rows.forEach((row, index) => {
            expect(source.indexOfKey(row.id, index + 3)).toBe(index);
          });

          sections = next;
          previousRows = reference.nextRows;
          previousFlat = rows;
          previousReference = reference.rows;
        }
      }
    });
  }

  it('sends only the items of changed sections and reuses the rest natively', () => {
    const { store, module } = fakeStore();
    const state = createSectionSourceState<Item, object>();
    const shape = { hasHeader: true, hasFooter: false, stickyEnabled: true };
    const a: SectionListData<Item, object> = {
      key: 'a',
      data: [{ id: '1' }, { id: '2' }],
    };
    const b: SectionListData<Item, object> = { key: 'b', data: [{ id: '1' }] };
    const first = buildSectionSource(state, [a, b], undefined, shape, store);
    const setSections = jestSpy(module, 'setSections');
    const b2 = { ...b, data: [...b.data, { id: '9' }] };
    const nextSections = [a, b2];
    const second = buildSectionSource(
      state,
      nextSections,
      undefined,
      shape,
      store
    );
    expect(setSections.calls[0]!.slice(2)).toEqual([
      first.keysVersion,
      1 + 4 + 8,
      ['a', 'b'],
      [2, 2],
      [0, -1],
      ['1', '9'],
    ]);
    expect(module.stores[0]!.versions.get(second.keysVersion)!.keys).toEqual([
      headerRowKey('a'),
      itemRowKey('a', '1'),
      itemRowKey('a', '2'),
      headerRowKey('b'),
      itemRowKey('b', '1'),
      itemRowKey('b', '9'),
    ]);
    // The same inputs again give the same source and write nothing.
    expect(
      buildSectionSource(state, nextSections, undefined, shape, store)
    ).toBe(second);
  });
});

/*
 * Records a module function's arguments and still calls it.
 */
function jestSpy<T extends object, K extends keyof T>(
  target: T,
  name: K
): { calls: unknown[][] } {
  const record = { calls: [] as unknown[][] };
  const original = target[name] as unknown as (...args: unknown[]) => unknown;
  (target as Record<K, unknown>)[name] = (...args: unknown[]) => {
    record.calls.push(args);
    return original(...args);
  };
  return record;
}
