/*
 * A stable hash, so a name keeps its color across sessions and list positions.
 */
export function getAvatarColor(
  name: string,
  palette: ReadonlyArray<string>
): string | undefined {
  let hash = 0;
  for (let index = 0; index < name.length; index++) {
    hash = (hash * 31 + name.charCodeAt(index)) % 2147483647;
  }
  return palette[hash % palette.length];
}

export function getInitials(name: string): string {
  const words = name.trim().split(/\s+/).filter(Boolean);
  const first = words[0] ?? '';
  const last = words.length > 1 ? words[words.length - 1]! : '';
  // Spreading keeps emoji and other multi unit characters whole.
  return `${[...first][0] ?? ''}${[...last][0] ?? ''}`.toUpperCase();
}
