import { collectExpandableIds } from '../collectExpandableIds';
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

describe('collectExpandableIds', () => {
  it('returns every node with children, at any depth', () => {
    const tree: Node[] = [
      {
        id: 'root',
        children: [{ id: 'leaf' }, { id: 'dir', children: [{ id: 'x' }] }],
      },
      { id: 'empty', children: [] },
    ];
    const ids = collectExpandableIds(tree, {
      getChildren: (node) => node.children,
      keyExtractor: (node) => node.id,
    });
    expect(ids.sort()).toEqual(['dir', 'root']);
  });

  it('handles deep trees without recursion', () => {
    let node: Node = { id: 'leaf' };
    for (let depth = 0; depth < 50_000; depth++) {
      node = { id: `n${depth}`, children: [node] };
    }
    const ids = collectExpandableIds([node], {
      getChildren: (n) => n.children,
      keyExtractor: (n) => n.id,
    });
    expect(ids).toHaveLength(50_000);
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
      firstIndex: 2,
      lastIndex: 7,
    });
  });
});
