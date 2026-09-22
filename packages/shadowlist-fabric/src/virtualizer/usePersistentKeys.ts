import { useMemo } from 'react';

interface UsePersistentKeysOptions {
  keys: ReadonlyArray<string>;
  persistentKeys: ReadonlyArray<string> | undefined;
  renderIndices: number[];
}

/*
 * Keeps the rows in persistentKeys always mounted, in their normal place in the list.
 * Unlike sticky headers, they are not pinned to the screen.
 */
export function usePersistentKeys({
  keys,
  persistentKeys,
  renderIndices,
}: UsePersistentKeysOptions): number[] {
  // Find the current index of each key, dropping keys that are no longer in data.
  const persistentIndices = useMemo(() => {
    if (!persistentKeys || persistentKeys.length === 0) return [];
    const keySet = new Set(persistentKeys);
    const indices: number[] = [];
    for (let index = 0; index < keys.length; index++) {
      if (keySet.has(keys[index]!)) indices.push(index);
    }
    return indices;
  }, [keys, persistentKeys]);

  // Add these rows to the rendered set, sorted and without duplicates.
  return useMemo(() => {
    if (persistentIndices.length === 0) return renderIndices;
    const merged = new Set(renderIndices);
    let changed = false;
    for (const index of persistentIndices) {
      if (!merged.has(index)) {
        merged.add(index);
        changed = true;
      }
    }
    if (!changed) return renderIndices;
    return Array.from(merged).sort((a, b) => a - b);
  }, [renderIndices, persistentIndices]);
}
