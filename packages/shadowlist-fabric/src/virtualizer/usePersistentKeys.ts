import { useMemo, useRef } from 'react';
import { mergeSortedIndices } from './helpers';
import { keysIdentityOf, type RowSource } from './rowSource';

interface UsePersistentKeysOptions {
  source: RowSource<unknown>;
  persistentKeys: ReadonlyArray<string> | undefined;
  renderIndices: number[];
}

/*
 * Keeps the rows in persistentKeys always mounted, in their normal place in the list.
 * Unlike sticky headers, they are not pinned to the screen.
 */
export function usePersistentKeys({
  source,
  persistentKeys,
  renderIndices,
}: UsePersistentKeysOptions): number[] {
  // Find the current index of each key, dropping keys that are no longer in data.
  // Where each key was last time, which is where the lookup starts.
  const hintsRef = useRef(new Map<string, number>());
  const sourceRef = useRef(source);
  sourceRef.current = source;
  const keysIdentity = keysIdentityOf(source);
  const persistentIndices = useMemo(() => {
    if (!persistentKeys || persistentKeys.length === 0) return [];
    const indices: number[] = [];
    const hints = new Map<string, number>();
    for (const key of persistentKeys) {
      const index = sourceRef.current.indexOfKey(
        key,
        hintsRef.current.get(key)
      );
      if (index >= 0) {
        indices.push(index);
        hints.set(key, index);
      }
    }
    hintsRef.current = hints;
    indices.sort((a, b) => a - b);
    return indices.filter(
      (index, position) => position === 0 || indices[position - 1] !== index
    );
    // keysIdentity stands for the source: the indices only change with the keys.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [keysIdentity, persistentKeys]);

  // Add these rows to the rendered set, sorted and without duplicates.
  return useMemo(
    () => mergeSortedIndices(renderIndices, persistentIndices),
    [renderIndices, persistentIndices]
  );
}
