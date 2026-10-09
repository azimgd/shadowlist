/*
 * The selected rows of a list by key, the same rules as host/ListSelection.cpp. A selection
 * follows its rows across inserts and moves, and a removed row leaves it. With single
 * selection, selecting a row deselects the one selected before. Each function returns the
 * array it was given when nothing changed.
 */
export function selectKey(
  selected: ReadonlyArray<string>,
  key: string,
  multiple: boolean
): ReadonlyArray<string> {
  if (selected.includes(key)) {
    return multiple || selected.length === 1 ? selected : [key];
  }
  return multiple ? [...selected, key] : [key];
}

export function deselectKey(
  selected: ReadonlyArray<string>,
  key: string
): ReadonlyArray<string> {
  return selected.includes(key)
    ? selected.filter((selectedKey) => selectedKey !== key)
    : selected;
}

export function retainKeys(
  selected: ReadonlyArray<string>,
  keyToIndex: ReadonlyMap<string, number>
): ReadonlyArray<string> {
  return selected.every((key) => keyToIndex.has(key))
    ? selected
    : selected.filter((key) => keyToIndex.has(key));
}

/*
 * Positions of the selected keys, low to high.
 */
export function selectedIndices(
  selected: ReadonlyArray<string>,
  keyToIndex: ReadonlyMap<string, number>
): number[] {
  const indices: number[] = [];
  for (const key of selected) {
    const index = keyToIndex.get(key);
    if (index !== undefined) indices.push(index);
  }
  return indices.sort((a, b) => a - b);
}
