import { describe, expect, it } from '@jest/globals';
import { flattenSections } from '../flattenSections';

interface Item {
  id: string;
}

const section = (key: string, ids: string[], extra: object = {}) => ({
  key,
  data: ids.map((id) => ({ id })),
  ...extra,
});

const SECTIONS = [section('A', ['a1', 'a2']), section('B', ['b1'])];

const flatten = (
  overrides: Partial<Parameters<typeof flattenSections<Item>>[0]> = {}
) =>
  flattenSections<Item>({
    sections: SECTIONS,
    keyExtractor: undefined,
    hasSectionHeader: true,
    hasSectionFooter: true,
    stickySectionHeadersEnabled: true,
    ...overrides,
  });

describe('flattenSections', () => {
  it('flattens header, items, footer per section in order', () => {
    const { rows } = flatten();
    expect(rows.map((row) => `${row.type}:${row.id}`)).toEqual([
      'sectionHeader:sh:A',
      'item:si:A:a1',
      'item:si:A:a2',
      'sectionFooter:sf:A',
      'sectionHeader:sh:B',
      'item:si:B:b1',
      'sectionFooter:sf:B',
    ]);
  });

  it('tags rows with their section index and item index', () => {
    const { rows } = flatten();
    const a2 = rows.find((row) => row.id === 'si:A:a2')!;
    expect(a2.sectionIndex).toBe(0);
    expect(a2.itemIndex).toBe(1);
    expect(a2.item).toEqual({ id: 'a2' });
  });

  it('collects sticky indices at section-header rows', () => {
    expect(flatten().stickyHeaderIndices).toEqual([0, 4]);
  });

  it('collects no sticky indices when sticky headers are disabled', () => {
    expect(
      flatten({ stickySectionHeadersEnabled: false }).stickyHeaderIndices
    ).toEqual([]);
  });

  it('emits no header rows (nor sticky indices) without a header renderer', () => {
    const { rows, stickyHeaderIndices } = flatten({ hasSectionHeader: false });
    expect(rows.some((row) => row.type === 'sectionHeader')).toBe(false);
    expect(stickyHeaderIndices).toEqual([]);
  });

  it('falls back from section key to a positional key', () => {
    const { rows } = flattenSections<Item>({
      sections: [{ data: [{ id: 'x' }] }],
      hasSectionHeader: true,
      hasSectionFooter: false,
      stickySectionHeadersEnabled: true,
    });
    expect(rows[0]!.id).toBe('sh:section-0');
    expect(rows[1]!.id).toBe('si:section-0:x');
  });

  it('prefers the per-section keyExtractor, then the list one, then item.id', () => {
    const { rows } = flattenSections<Item>({
      sections: [
        section('A', ['a1'], {
          keyExtractor: (item: Item) => `sec-${item.id}`,
        }),
        section('B', ['b1']),
      ],
      keyExtractor: (item) => `list-${item.id}`,
      hasSectionHeader: false,
      hasSectionFooter: false,
      stickySectionHeadersEnabled: false,
    });
    expect(rows[0]!.id).toBe('si:A:sec-a1');
    expect(rows[1]!.id).toBe('si:B:list-b1');
  });

  it('falls back to the item index when an item has no id', () => {
    const { rows } = flattenSections<{ name: string }>({
      sections: [{ key: 'A', data: [{ name: 'n' }] }],
      hasSectionHeader: false,
      hasSectionFooter: false,
      stickySectionHeadersEnabled: false,
    });
    expect(rows[0]!.id).toBe('si:A:0');
  });

  describe('separators', () => {
    it('puts item separators between items, none on headers', () => {
      const { rows } = flatten();
      const byId = new Map(rows.map((row) => [row.id, row.separator]));
      expect(byId.get('sh:A')).toBeNull();
      expect(byId.get('si:A:a1')).toBe('item');
      expect(byId.get('si:A:a2')).toBeNull(); // footer owns the boundary
    });

    it('puts the section separator on footers of non-final sections', () => {
      const { rows } = flatten();
      const byId = new Map(rows.map((row) => [row.id, row.separator]));
      expect(byId.get('sf:A')).toBe('section');
      expect(byId.get('sf:B')).toBeNull(); // final section
    });

    it('moves the section separator onto the last item when no footer renders', () => {
      const { rows } = flatten({ hasSectionFooter: false });
      const byId = new Map(rows.map((row) => [row.id, row.separator]));
      expect(byId.get('si:A:a2')).toBe('section');
      expect(byId.get('si:B:b1')).toBeNull(); // final section
    });
  });
});
