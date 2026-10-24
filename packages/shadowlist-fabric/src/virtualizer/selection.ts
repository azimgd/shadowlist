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

export function selectOnlyKey(
  selected: ReadonlyArray<string>,
  key: string
): ReadonlyArray<string> {
  return selected.length === 1 && selected[0] === key ? selected : [key];
}

export function deselectKey(
  selected: ReadonlyArray<string>,
  key: string
): ReadonlyArray<string> {
  return selected.includes(key)
    ? selected.filter((selectedKey) => selectedKey !== key)
    : selected;
}

/*
 * indexOfKey gives a key's row or -1.
 */
export function retainKeys(
  selected: ReadonlyArray<string>,
  indexOfKey: (key: string) => number
): ReadonlyArray<string> {
  return selected.every((key) => indexOfKey(key) >= 0)
    ? selected
    : selected.filter((key) => indexOfKey(key) >= 0);
}

/*
 * Positions of the selected keys, low to high.
 */
export function selectedIndices(
  selected: ReadonlyArray<string>,
  indexOfKey: (key: string) => number
): number[] {
  const indices: number[] = [];
  for (const key of selected) {
    const index = indexOfKey(key);
    if (index >= 0) indices.push(index);
  }
  return indices.sort((a, b) => a - b);
}
