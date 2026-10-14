import {
  AVATAR_COLORS,
  AVATAR_NAMES,
  CHARACTER_NAMES,
  SAMPLE_TEXTS,
  SECTION_TITLES,
} from './FixtureStrings.js';

/*
 * The example data, item for item the same as the UIKit and Android examples' fixtures.
 * Every value is a pure function of the item's generation index.
 */
export function avatarColor(name: string): string {
  let hash = 0;
  for (let at = 0; at < name.length; ++at) {
    hash = (hash * 31 + name.charCodeAt(at)) % 2_147_483_647;
  }
  return AVATAR_COLORS[hash % AVATAR_COLORS.length]!;
}

export function initials(name: string): string {
  const words = name.split(' ').filter((word) => word.length > 0);
  const first = words[0]?.[0];
  if (!first) return '';
  if (words.length > 1)
    return `${first}${words[words.length - 1]![0]}`.toUpperCase();
  return first.toUpperCase();
}

export class FeedPost {
  readonly id: string;
  readonly name: string;
  readonly handle: string;
  readonly text: string;
  readonly time: string;

  constructor(index: number) {
    this.id = `post-${index}`;
    this.name = CHARACTER_NAMES[index % CHARACTER_NAMES.length]!;
    this.handle = '@' + this.name.toLowerCase().replaceAll(' ', '');
    this.text = SAMPLE_TEXTS[index % SAMPLE_TEXTS.length]!;
    const hours = index % 24;
    this.time = hours === 0 ? 'now' : `${hours}h`;
  }
}

export class ChatMessage {
  readonly id: string;
  readonly isOwn: boolean;
  readonly name: string;
  readonly text: string;

  constructor(index: number, id = `msg-${index}`) {
    this.id = id;
    this.isOwn = index % 3 !== 0;
    this.name = this.isOwn ? 'Me' : AVATAR_NAMES[index % AVATAR_NAMES.length]!;
    this.text = SAMPLE_TEXTS[index % SAMPLE_TEXTS.length]!;
  }
}

export class Tile {
  readonly id: string;
  readonly title: string;
  readonly color: string;
  readonly size: number;

  constructor(index: number) {
    this.id = `tile-${index}`;
    this.title = SECTION_TITLES[index % SECTION_TITLES.length]!;
    this.color = AVATAR_COLORS[index % AVATAR_COLORS.length]!;
    this.size = 120 + ((index * 37) % 140);
  }
}
