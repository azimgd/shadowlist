import { ShadowListKitListCell } from 'shadowlist-wasm';

import {
  avatarColor,
  initials,
  type ChatMessage,
  type FeedPost,
  type Tile,
} from './Fixtures.js';

/*
 * A post with its author and text. Its height comes from measuring the text.
 */
export class FeedCell extends ShadowListKitListCell {
  static readonly IDENTIFIER = 'feed';

  private readonly avatar = document.createElement('div');
  private readonly name = document.createElement('div');
  private readonly meta = document.createElement('div');
  private readonly text = document.createElement('p');

  constructor() {
    super(FeedCell.IDENTIFIER);
    this.view.className = 'feed-cell';
    this.avatar.className = 'avatar';
    this.name.className = 'name';
    this.meta.className = 'meta';
    this.text.className = 'text';
    const header = document.createElement('div');
    header.className = 'feed-header';
    const titles = document.createElement('div');
    titles.append(this.name, this.meta);
    header.append(this.avatar, titles);
    this.view.append(header, this.text);
  }

  configure(post: FeedPost): void {
    this.avatar.textContent = initials(post.name);
    this.avatar.style.background = avatarColor(post.name);
    this.name.textContent = post.name;
    this.meta.textContent = `${post.handle} · ${post.time}`;
    this.text.textContent = post.text;
  }
}

/*
 * A chat bubble, on the trailing side for the reader's own messages.
 */
export class ChatCell extends ShadowListKitListCell {
  static readonly IDENTIFIER = 'chat';

  private readonly bubble = document.createElement('div');

  constructor() {
    super(ChatCell.IDENTIFIER);
    this.view.className = 'chat-cell';
    this.bubble.className = 'bubble';
    this.view.append(this.bubble);
  }

  configure(message: ChatMessage): void {
    this.view.classList.toggle('own', message.isOwn);
    this.bubble.textContent = message.isOwn
      ? message.text
      : `${message.name}: ${message.text}`;
  }
}

/*
 * A colored tile with a title, sized by the data source.
 */
export class TileCell extends ShadowListKitListCell {
  static readonly IDENTIFIER = 'tile';

  constructor() {
    super(TileCell.IDENTIFIER);
    this.view.className = 'tile-cell';
  }

  configure(tile: Tile, index: number): void {
    this.view.style.background = tile.color;
    this.view.textContent = `${index} · ${tile.title}`;
  }
}

/*
 * A section title that sticks while its rows scroll under it.
 */
export class HeaderCell extends ShadowListKitListCell {
  static readonly IDENTIFIER = 'header';

  constructor() {
    super(HeaderCell.IDENTIFIER);
    this.view.className = 'header-cell';
  }

  configure(title: string): void {
    this.view.textContent = title;
  }
}
