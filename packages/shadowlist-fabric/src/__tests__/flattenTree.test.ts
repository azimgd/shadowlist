import { describe, expect, it } from '@jest/globals';
import { flattenTree } from '../flattenTree';

interface Node {
  id: string;
  children?: Node[];
}

const node = (id: string, children?: Node[]): Node => ({ id, children });
const getChildren = (item: Node) => item.children;
const keyOf = (item: Node) => item.id;

/*
 * a
 * ├─ a1
 * │  └─ a1x
 * └─ a2
 * b
 */
const TREE: Node[] = [
  node('a', [node('a1', [node('a1x')]), node('a2')]),
  node('b'),
];

const flatten = (expanded: string[]) =>
  flattenTree(TREE, getChildren, keyOf, new Set(expanded));

describe('flattenTree', () => {
  it('lists only roots when nothing is expanded', () => {
    const { rows } = flatten([]);
    expect(rows.map((row) => row.id)).toEqual(['a', 'b']);
    expect(rows[0]).toMatchObject({
      depth: 0,
      hasChildren: true,
      isExpanded: false,
    });
    expect(rows[1]).toMatchObject({ hasChildren: false, isExpanded: false });
  });

  it('descends expanded nodes in pre-order with depths', () => {
    const { rows } = flatten(['a', 'a1']);
    expect(rows.map((row) => `${row.id}@${row.depth}`)).toEqual([
      'a@0',
      'a1@1',
      'a1x@2',
      'a2@1',
      'b@0',
    ]);
  });

  it('never descends a collapsed subtree, even with expanded descendants', () => {
    // 'a1' is marked expanded but its parent 'a' is collapsed.
    const { rows } = flatten(['a1']);
    expect(rows.map((row) => row.id)).toEqual(['a', 'b']);
  });

  it('ignores an expanded id on a leaf', () => {
    const { rows } = flatten(['a', 'b']);
    const b = rows.find((row) => row.id === 'b')!;
    expect(b.isExpanded).toBe(false);
  });

  it('treats an empty children array as a leaf', () => {
    const { rows } = flattenTree(
      [{ id: 'x', children: [] }],
      getChildren,
      keyOf,
      new Set(['x'])
    );
    expect(rows[0]).toMatchObject({ hasChildren: false, isExpanded: false });
  });

  it('maps every visible id to its flat index', () => {
    const { rows, indexByKey } = flatten(['a']);
    rows.forEach((row, index) => {
      expect(indexByKey.get(row.id)).toBe(index);
    });
    expect(indexByKey.has('a1x')).toBe(false); // collapsed away
  });

  it('flattens an empty tree', () => {
    const { rows, indexByKey } = flattenTree([], getChildren, keyOf, new Set());
    expect(rows).toEqual([]);
    expect(indexByKey.size).toBe(0);
  });
});
