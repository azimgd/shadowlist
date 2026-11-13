import type { ColorValue } from 'react-native';

const REPLACEMENT_UNIT = 0xfffd;

function isHighSurrogate(unit: number): boolean {
  return unit >= 0xd800 && unit <= 0xdbff;
}

function isLowSurrogate(unit: number): boolean {
  return unit >= 0xdc00 && unit <= 0xdfff;
}

/*
 * A stable hash over the name's UTF-16 units. A name keeps its color across sessions and list
 * positions. Half a surrogate pair hashes as U+FFFD, which is what it turns into on its way
 * through UTF-8 on the other platforms. avatarColorIndex in the core's utils/Avatar.hpp is the
 * same rule, and vectors.test holds them equal.
 */
export function getAvatarColor(
  name: string,
  palette: ReadonlyArray<ColorValue>
): ColorValue | undefined {
  let hash = 0;
  for (let index = 0; index < name.length; index++) {
    let unit = name.charCodeAt(index);
    const paired =
      isHighSurrogate(unit) && isLowSurrogate(name.charCodeAt(index + 1));
    if (paired) {
      hash = (hash * 31 + unit) % 2147483647;
      index++;
      unit = name.charCodeAt(index);
    } else if (isHighSurrogate(unit) || isLowSurrogate(unit)) {
      unit = REPLACEMENT_UNIT;
    }
    hash = (hash * 31 + unit) % 2147483647;
  }
  return palette[hash % palette.length];
}

/*
 * The first character of a word, with half a surrogate pair read as U+FFFD.
 */
function firstCharacter(word: string): string {
  // Spreading keeps emoji and other multi unit characters whole.
  const first = [...word][0] ?? '';
  const unit = first.length === 1 ? first.charCodeAt(0) : 0;
  return isHighSurrogate(unit) || isLowSurrogate(unit)
    ? String.fromCharCode(REPLACEMENT_UNIT)
    : first;
}

export function getInitials(name: string): string {
  const words = name.trim().split(/\s+/).filter(Boolean);
  const first = words[0] ?? '';
  const last = words.length > 1 ? words[words.length - 1]! : '';
  return `${firstCharacter(first)}${firstCharacter(last)}`.toUpperCase();
}

/*
 * The font size of the initials in an avatar of size, a whole number. avatarInitialsMetrics in
 * the core's utils/Avatar.hpp is the same rule, and vectors.test holds them equal.
 */
export function getInitialsFontSize(size: number): number {
  return Math.floor(size * 0.43);
}
