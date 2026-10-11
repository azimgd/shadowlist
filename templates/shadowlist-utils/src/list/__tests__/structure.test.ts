import { collectExpandableKeys } from '../collectExpandableKeys';
import { getViewableRange } from '../getViewableRange';
import { groupIntoSections } from '../groupIntoSections';

interface Node {
  id: string;
  children?: Node[];
}

describe('groupIntoSections', () => {
  it('groups by title and sorts sections', () => {
    const sections = groupIntoSections(['bob', 'alice', 'bea', 'al'], {
      getSectionTitle: (name) => name.charAt(0).toUpperCase(),
      compareItems: (x, y) => x.localeCompare(y),
    });
    expect(sections).toEqual([
      { key: 'A', title: 'A', data: ['al', 'alice'] },
      { key: 'B', title: 'B', data: ['bea', 'bob'] },
    ]);
  });

  it('does not reorder the input array', () => {
    const input = ['b', 'a'];
    groupIntoSections(input, {
      getSectionTitle: () => 'x',
      compareItems: (x, y) => x.localeCompare(y),
    });
    expect(input).toEqual(['b', 'a']);
  });
});

describe('collectExpandableKeys', () => {
  it('returns every node with children, at any depth', () => {
    const tree: Node[] = [
      {
        id: 'root',
        children: [{ id: 'leaf' }, { id: 'dir', children: [{ id: 'x' }] }],
      },
      { id: 'empty', children: [] },
    ];
    const keys = collectExpandableKeys(tree, {
      getChildren: (node) => node.children,
      keyExtractor: (node) => node.id,
    });
    expect(keys.sort()).toEqual(['dir', 'root']);
  });

  it('handles deep trees without recursion', () => {
    let node: Node = { id: 'leaf' };
    for (let depth = 0; depth < 50_000; depth++) {
      node = { id: `n${depth}`, children: [node] };
    }
    const keys = collectExpandableKeys([node], {
      getChildren: (n) => n.children,
      keyExtractor: (n) => n.id,
    });
    expect(keys).toHaveLength(50_000);
  });
});

describe('getViewableRange', () => {
  it('is undefined when nothing is viewable', () => {
    expect(getViewableRange([])).toBeUndefined();
  });

  it('does not assume sorted input', () => {
    expect(
      getViewableRange([{ index: 7 }, { index: 2 }, { index: 5 }])
    ).toEqual({
      low: 2,
      high: 7,
    });
  });
});
