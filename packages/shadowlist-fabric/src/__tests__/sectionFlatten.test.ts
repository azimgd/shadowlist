import { describe, expect, it } from '@jest/globals';
import {
  flattenSections,
  type FlatRow,
  type SectionRows,
} from '../virtualizer/sectionRows';
import type { SectionListData } from '../types';

interface Item {
  id: string;
}

type Section = SectionListData<Item, { title: string }>;
type Row = FlatRow<Item, { title: string }>;

/*
 * The flatten SectionList used before sections were cached: one pass over every element,
 * reusing a row by id when all its fields match.
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
      previous.element === row.element &&
      previous.elementIndex === row.elementIndex &&
      previous.elementKey === row.elementKey &&
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
        id: `sh:${sectionKey}`,
        type: 'sectionHeader',
        section,
        sectionIndex,
      });
    }
    const last = section.data.length - 1;
    section.data.forEach((element, elementIndex) => {
      const isLastInSection = elementIndex === last;
      push({
        id: `si:${sectionKey}:${element.id}`,
        type: 'element',
        sectionIndex,
        element,
        elementIndex,
        elementKey: element.id,
        isLastInSection,
        isSectionBoundary:
          isLastInSection && !hasFooter && sectionIndex < sections.length - 1,
      });
    });
    if (hasFooter) {
      push({
        id: `sf:${sectionKey}`,
        type: 'sectionFooter',
        section,
        sectionIndex,
        isSectionBoundary: sectionIndex < sections.length - 1,
      });
    }
  });
  return { rows, stickyIndices, nextRows };
}

describe('flattenSections', () => {
  it('matches the reference flatten and reuses the same rows', () => {
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
      let sections: Section[] = [section(3), section(0), section(4)];
      let previousSections = new Map<
        string,
        SectionRows<Item, { title: string }>
      >();
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

        const result = flattenSections(
          next,
          undefined,
          hasHeader,
          hasFooter,
          true,
          previousSections
        );
        const reference = referenceFlatten(
          next,
          hasHeader,
          hasFooter,
          previousRows
        );
        expect(result.rows).toEqual(reference.rows);
        expect(result.stickyIndices).toEqual(reference.stickyIndices);
        // A row keeps its object exactly when the reference kept it.
        const kept = (rows: Row[], before: Row[]) => {
          const byId = new Map(before.map((row) => [row.id, row]));
          return rows.map((row) => byId.get(row.id) === row);
        };
        expect(kept(result.rows, previousFlat)).toEqual(
          kept(reference.rows, previousReference)
        );

        sections = next;
        previousSections = result.nextSections;
        previousRows = reference.nextRows;
        previousFlat = result.rows;
        previousReference = reference.rows;
      }
    }
  });
});
