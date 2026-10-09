import {
  appendInfiniteItems,
  flattenInfiniteItems,
  prependInfiniteItems,
  removeInfiniteItems,
  trimInfinitePages,
  updateInfiniteItems,
  upsertInfiniteItems,
} from '../infiniteItems';

interface Row {
  id: string;
  votes: number;
}

const row = (id: string, votes = 0): Row => ({ id, votes });

const makeData = () => ({
  pages: [{ items: [row('a'), row('b')] }, { items: [row('c')] }],
  pageParams: [0, 2],
});

describe('flattenInfiniteItems', () => {
  it('returns one shared empty array while nothing is cached', () => {
    expect(flattenInfiniteItems(undefined)).toBe(
      flattenInfiniteItems(undefined)
    );
    expect(flattenInfiniteItems(undefined)).toHaveLength(0);
  });

  it('returns a single page items array as-is', () => {
    const data = { pages: [{ items: [row('a')] }], pageParams: [0] };
    expect(flattenInfiniteItems(data)).toBe(data.pages[0]!.items);
  });

  it('concatenates pages in order', () => {
    expect(flattenInfiniteItems(makeData()).map((r) => r.id)).toEqual([
      'a',
      'b',
      'c',
    ]);
  });
});

describe('updateInfiniteItems', () => {
  it('returns data itself when nothing changed', () => {
    const data = makeData();
    expect(updateInfiniteItems(data, (r) => r)).toBe(data);
  });

  it('keeps untouched pages and rows by identity', () => {
    const data = makeData();
    const next = updateInfiniteItems(data, (r) =>
      r.id === 'c' ? { ...r, votes: 1 } : r
    )!;
    expect(next).not.toBe(data);
    expect(next.pages[0]).toBe(data.pages[0]);
    expect(next.pages[1]!.items[0]).toEqual(row('c', 1));
  });

  it('passes undefined through', () => {
    const data: ReturnType<typeof makeData> | undefined = undefined;
    expect(updateInfiniteItems(data, (r) => r)).toBeUndefined();
  });
});

describe('removeInfiniteItems', () => {
  it('removes matches from whichever page holds them', () => {
    const data = makeData();
    const next = removeInfiniteItems(data, (r) => r.id === 'b')!;
    expect(flattenInfiniteItems(next).map((r) => r.id)).toEqual(['a', 'c']);
    expect(next.pages[1]).toBe(data.pages[1]);
  });

  it('returns data itself when nothing matched', () => {
    const data = makeData();
    expect(removeInfiniteItems(data, () => false)).toBe(data);
  });
});

describe('prepend / append', () => {
  it('prepends to the first page only', () => {
    const data = makeData();
    const next = prependInfiniteItems(data, [row('z')])!;
    expect(next.pages[0]!.items.map((r) => r.id)).toEqual(['z', 'a', 'b']);
    expect(next.pages[1]).toBe(data.pages[1]);
  });

  it('appends to the last page only', () => {
    const data = makeData();
    const next = appendInfiniteItems(data, [row('z')])!;
    expect(next.pages[1]!.items.map((r) => r.id)).toEqual(['c', 'z']);
    expect(next.pages[0]).toBe(data.pages[0]);
  });

  it('returns data itself for an empty insert', () => {
    const data = makeData();
    expect(prependInfiniteItems(data, [])).toBe(data);
    expect(appendInfiniteItems(data, [])).toBe(data);
  });
});

describe('upsertInfiniteItems', () => {
  it('replaces existing ids in place and appends new ones to the last page', () => {
    const data = makeData();
    const next = upsertInfiniteItems(data, [row('a', 5), row('z')])!;
    expect(flattenInfiniteItems(next)).toEqual([
      row('a', 5),
      row('b'),
      row('c'),
      row('z'),
    ]);
    expect(next.pages[0]!.items[1]).toBe(data.pages[0]!.items[1]);
  });

  it('never duplicates an id that is already cached', () => {
    const data = makeData();
    const next = upsertInfiniteItems(data, [row('c', 1)])!;
    expect(flattenInfiniteItems(next).map((r) => r.id)).toEqual([
      'a',
      'b',
      'c',
    ]);
    expect(next.pages[0]).toBe(data.pages[0]);
  });

  it('returns data itself when every row is already the same object', () => {
    const data = makeData();
    expect(upsertInfiniteItems(data, [data.pages[1]!.items[0]!])).toBe(data);
  });
});

describe('trimInfinitePages', () => {
  it('keeps the first pages and their params', () => {
    const next = trimInfinitePages(makeData(), 1)!;
    expect(next.pages).toHaveLength(1);
    expect(next.pageParams).toEqual([0]);
  });

  it('returns data itself when already short enough', () => {
    const data = makeData();
    expect(trimInfinitePages(data, 2)).toBe(data);
  });
});
