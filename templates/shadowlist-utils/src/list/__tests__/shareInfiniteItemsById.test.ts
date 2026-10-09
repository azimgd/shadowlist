import {
  shareInfiniteItemsById,
  shareItemsById,
} from '../shareInfiniteItemsById';

const post = (id: string, title = id) => ({ id, title, tags: [id] });

describe('shareInfiniteItemsById', () => {
  it('returns previous itself when the data is value-equal', () => {
    const previous = { pages: [{ items: [post('a')] }], pageParams: [0] };
    const next = { pages: [{ items: [post('a')] }], pageParams: [0] };
    expect(shareInfiniteItemsById(previous, next)).toBe(previous);
  });

  it('keeps row identity when a page of history is loaded in front', () => {
    const a = post('a');
    const b = post('b');
    const previous = { pages: [{ items: [a, b] }], pageParams: [10] };
    const next = {
      pages: [
        { items: [post('y'), post('z')] },
        { items: [post('a'), post('b')] },
      ],
      pageParams: [8, 10],
    };
    const shared = shareInfiniteItemsById(previous, next);
    expect(shared).not.toBe(previous);
    expect(shared.pages[1]).toBe(previous.pages[0]);
    expect(shared.pages[1]!.items[0]).toBe(a);
  });

  it('keeps unchanged rows and replaces changed ones', () => {
    const a = post('a');
    const previous = { pages: [{ items: [a, post('b')] }], pageParams: [0] };
    const next = {
      pages: [{ items: [post('a'), post('b', 'edited')] }],
      pageParams: [0],
    };
    const shared = shareInfiniteItemsById(previous, next);
    expect(shared.pages[0]!.items[0]).toBe(a);
    expect(shared.pages[0]!.items[1]).toBe(next.pages[0]!.items[1]);
  });

  it('passes non-infinite values through', () => {
    const next = { foo: 1 };
    expect(shareInfiniteItemsById({ foo: 1 }, next)).toBe(next);
  });
});

describe('shareItemsById', () => {
  it('shares moved rows in a plain array', () => {
    const a = post('a');
    const b = post('b');
    const shared = shareItemsById([a, b], [post('new'), post('a'), post('b')]);
    expect(shared[1]).toBe(a);
    expect(shared[2]).toBe(b);
  });

  it('returns previous itself when nothing changed', () => {
    const previous = [post('a'), post('b')];
    expect(shareItemsById(previous, [post('a'), post('b')])).toBe(previous);
  });
});
