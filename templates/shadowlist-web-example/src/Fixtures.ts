import {
  avatarColorIndex,
  avatarInitials,
  relativeTime,
} from 'shadowlist-wasm/utils';

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
  return AVATAR_COLORS[avatarColorIndex(name, AVATAR_COLORS.length)]!;
}

export function initials(name: string): string {
  return avatarInitials(name).toLocaleUpperCase();
}

/*
 * A fixed fixture clock keeps each generation index deterministic across route changes.
 */
export const FEED_NOW_MS = 1_762_905_600_000;

export function feedTime(createdAtMs: number): string {
  const time = relativeTime(createdAtMs, FEED_NOW_MS);
  switch (time.unit) {
    case 'now':
      return 'now';
    case 'minutes':
      return `${time.count}m`;
    case 'hours':
      return `${time.count}h`;
    case 'days':
      return `${time.count}d`;
    case 'date':
      return new Date(createdAtMs).toLocaleDateString();
  }
}

export class FeedPost {
  readonly id: string;
  readonly name: string;
  readonly handle: string;
  readonly text: string;
  readonly createdAtMs: number;

  constructor(index: number) {
    this.id = `post-${index}`;
    this.name = CHARACTER_NAMES[index % CHARACTER_NAMES.length]!;
    this.handle = '@' + this.name.toLowerCase().replaceAll(' ', '');
    this.text = SAMPLE_TEXTS[index % SAMPLE_TEXTS.length]!;
    const hours = index % 24;
    this.createdAtMs = FEED_NOW_MS - hours * 3_600_000;
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
