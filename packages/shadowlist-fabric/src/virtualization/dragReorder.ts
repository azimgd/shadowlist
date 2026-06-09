/* Move the item at `from` to `to`, returning a new array. Out-of-bounds or no-op
 * moves return an untouched copy. */
export const arrayMove = <T>(
  input: ReadonlyArray<T>,
  from: number,
  to: number
): T[] => {
  const next = input.slice();
  if (
    from < 0 ||
    from >= next.length ||
    to < 0 ||
    to >= next.length ||
    from === to
  ) {
    return next;
  }
  const moved = next.splice(from, 1)[0] as T;
  next.splice(to, 0, moved);
  return next;
};

/*
 * Union the dragged row's index into the rendered set so it stays mounted while
 * carried off-screen by virtualization. Returns the input array (same identity, so
 * memos hold) when there is nothing to add.
 */
export const unionDraggedIndex = (
  mountedIndices: number[],
  draggingIndex: number,
  dataLength: number
): number[] => {
  if (
    draggingIndex < 0 ||
    draggingIndex >= dataLength ||
    mountedIndices.includes(draggingIndex)
  ) {
    return mountedIndices;
  }
  return [...mountedIndices, draggingIndex].sort((a, b) => a - b);
};
