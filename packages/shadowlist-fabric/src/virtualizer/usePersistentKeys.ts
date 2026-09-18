import { useMemo } from 'react';

interface UsePersistentKeysOptions {
  keys: ReadonlyArray<string>;
  persistentKeys: ReadonlyArray<string> | undefined;
  renderIndices: number[];
}

/*
 * Persistent (always-mounted) rows: force-mounts the rows whose key is in
 * `persistentKeys` so they are never virtualized away, while leaving them at their
 * natural flow position (unlike sticky headers, which are pinned to the viewport).
 */
export function usePersistentKeys({
  keys,
  persistentKeys,
  renderIndices,
}: UsePersistentKeysOptions): number[] {
  /*
   * Resolve the requested keys to their current indices. Walks the keys only when keys are
   * given; keys not present in data are dropped (e.g. a pinned row that was removed).
   */
  const persistentIndices = useMemo(() => {
    if (!persistentKeys || persistentKeys.length === 0) return [];
    const keySet = new Set(persistentKeys);
    const indices: number[] = [];
    for (let index = 0; index < keys.length; index++) {
      if (keySet.has(keys[index]!)) indices.push(index);
    }
    return indices;
  }, [keys, persistentKeys]);

  // Union the persistent rows into the rendered set, kept sorted and deduplicated.
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
