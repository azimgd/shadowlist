import { pollTally } from 'shadowlist-wasm/utils';
import {
  ShadowListKitListView,
  type ShadowListKitListViewDataSource,
} from 'shadowlist-wasm';

import { PollCell } from './PollCell.js';
import type { PollData } from './PollData.js';
import { ChatMessage, FeedPost, Tile } from './Fixtures.js';
import { SECTION_TITLES } from './FixtureStrings.js';
import { ChatCell, FeedCell, HeaderCell, TileCell } from './Rows.js';
import { Settings } from './Settings.js';

/*
 * A screen fills the page body with a toolbar and one list.
 */
export type Screen = (body: HTMLElement) => ShadowListKitListView;

function toolbarButton(
  toolbar: HTMLElement,
  title: string,
  action: () => void
): void {
  const button = document.createElement('button');
  button.textContent = title;
  button.addEventListener('click', action);
  toolbar.append(button);
}

function makeList(body: HTMLElement): {
  list: ShadowListKitListView;
  toolbar: HTMLElement;
} {
  const toolbar = document.createElement('div');
  toolbar.className = 'toolbar';
  const list = new ShadowListKitListView();
  list.className = 'list';
  body.append(toolbar, list);
  return { list, toolbar };
}

/*
 * Posts measured through their cells. The toolbar prepends, appends and removes rows, which
 * keeps what is on screen in place, and runs the scroll commands.
 */
export const feedScreen: Screen = (body) => {
  const { list, toolbar } = makeList(body);
  let first = 0;
  let posts = Array.from(
    { length: Settings.count },
    (_, index) => new FeedPost(index)
  );
  list.registerCell(FeedCell.IDENTIFIER, () => new FeedCell());
  const dataSource: ShadowListKitListViewDataSource = {
    numberOfItems: () => posts.length,
    keyForItem: (_, index) => posts[index]!.id,
    cellForItem: (view, index) => {
      const cell = view.dequeueReusableCell<FeedCell>(FeedCell.IDENTIFIER);
      cell.configure(posts[index]!);
      return cell;
    },
  };
  list.dataSource = dataSource;
  const header = document.createElement('div');
  header.className = 'banner';
  header.textContent = 'Feed header';
  list.headerView = header;
  list.reloadData();
  toolbarButton(toolbar, 'Prepend 20', () => {
    first -= 20;
    posts = [
      ...Array.from(
        { length: 20 },
        (_, at) => new FeedPost(first + at + 100000)
      ),
      ...posts,
    ];
    list.insertItems(Array.from({ length: 20 }, (_, at) => at));
  });
  toolbarButton(toolbar, 'Append 20', () => {
    const start = posts.length;
    posts = [
      ...posts,
      ...Array.from(
        { length: 20 },
        (_, at) => new FeedPost(start + at + 200000)
      ),
    ];
    list.reloadData();
  });
  toolbarButton(toolbar, 'Remove first', () => {
    posts = posts.slice(1);
    list.deleteItems([0]);
  });
  toolbarButton(toolbar, 'Middle', () =>
    list.scrollToItem(Math.floor(posts.length / 2), 0.5, true)
  );
  toolbarButton(toolbar, 'Start', () => list.scrollToStart(true));
  toolbarButton(toolbar, 'End', () => list.scrollToEnd(true));
  return list;
};

/*
 * An inverted chat. It opens at the newest message, older messages load above when the reader
 * nears the start, and sending a message scrolls to it.
 */
export const chatScreen: Screen = (body) => {
  const { list, toolbar } = makeList(body);
  let oldest = 100000;
  let messages = Array.from(
    { length: Settings.count },
    (_, at) => new ChatMessage(oldest + at)
  );
  list.inverted = true;
  list.registerCell(ChatCell.IDENTIFIER, () => new ChatCell());
  list.dataSource = {
    numberOfItems: () => messages.length,
    keyForItem: (_, index) => messages[index]!.id,
    cellForItem: (view, index) => {
      const cell = view.dequeueReusableCell<ChatCell>(ChatCell.IDENTIFIER);
      cell.configure(messages[index]!);
      return cell;
    },
  };
  list.delegate = {
    didReachStart: (view) => {
      if (oldest <= 0) return;
      const count = Math.min(30, oldest);
      oldest -= count;
      messages = [
        ...Array.from(
          { length: count },
          (_, at) => new ChatMessage(oldest + at)
        ),
        ...messages,
      ];
      view.insertItems(Array.from({ length: count }, (_, at) => at));
    },
  };
  list.reloadData();
  let sent = 0;
  toolbarButton(toolbar, 'Send', () => {
    ++sent;
    messages = [...messages, new ChatMessage(sent * 3 + 1, `sent-${sent}`)];
    list.insertItems([messages.length - 1]);
    list.scrollToEnd(true);
  });
  toolbarButton(toolbar, 'Receive', () => {
    ++sent;
    messages = [...messages, new ChatMessage(sent * 3, `received-${sent}`)];
    list.insertItems([messages.length - 1]);
  });
  return list;
};

/*
 * A three column masonry grid with sizes from the data source. No row is measured.
 */
export const masonryScreen: Screen = (body) => {
  const { list } = makeList(body);
  const tiles = Array.from(
    { length: Settings.count },
    (_, index) => new Tile(index)
  );
  list.numberOfColumns = 3;
  list.registerCell(TileCell.IDENTIFIER, () => new TileCell());
  list.dataSource = {
    numberOfItems: () => tiles.length,
    keyForItem: (_, index) => tiles[index]!.id,
    sizeForItem: (_, index) => tiles[index]!.size,
    cellForItem: (view, index) => {
      const cell = view.dequeueReusableCell<TileCell>(TileCell.IDENTIFIER);
      cell.configure(tiles[index]!, index);
      return cell;
    },
  };
  list.reloadData();
  return list;
};

/*
 * A horizontal list of tiles that rests on a tile's start edge.
 */
export const horizontalScreen: Screen = (body) => {
  const { list } = makeList(body);
  const tiles = Array.from(
    { length: Settings.count },
    (_, index) => new Tile(index)
  );
  list.horizontal = true;
  list.snapToItem = true;
  list.registerCell(TileCell.IDENTIFIER, () => new TileCell());
  list.dataSource = {
    numberOfItems: () => tiles.length,
    keyForItem: (_, index) => tiles[index]!.id,
    sizeForItem: (_, index) => tiles[index]!.size + 60,
    cellForItem: (view, index) => {
      const cell = view.dequeueReusableCell<TileCell>(TileCell.IDENTIFIER);
      cell.configure(tiles[index]!, index);
      return cell;
    },
  };
  list.reloadData();
  return list;
};

/*
 * Posts grouped under titles every twenty rows. Each title sticks until the next one pushes it
 * away.
 */
export const stickyScreen: Screen = (body) => {
  const { list } = makeList(body);
  const SECTION = 20;
  const rows = Array.from({ length: Settings.count }, (_, index) =>
    index % SECTION === 0
      ? SECTION_TITLES[(index / SECTION) % SECTION_TITLES.length]!
      : new FeedPost(index)
  );
  list.registerCell(FeedCell.IDENTIFIER, () => new FeedCell());
  list.registerCell(HeaderCell.IDENTIFIER, () => new HeaderCell());
  list.dataSource = {
    numberOfItems: () => rows.length,
    keyForItem: (_, index) => {
      const row = rows[index]!;
      return typeof row === 'string' ? `section-${index}` : row.id;
    },
    cellForItem: (view, index) => {
      const row = rows[index]!;
      if (typeof row === 'string') {
        const header = view.dequeueReusableCell<HeaderCell>(
          HeaderCell.IDENTIFIER
        );
        header.configure(row);
        return header;
      }
      const cell = view.dequeueReusableCell<FeedCell>(FeedCell.IDENTIFIER);
      cell.configure(row);
      return cell;
    },
  };
  list.stickyIndices = rows.flatMap((row, index) =>
    typeof row === 'string' ? [index] : []
  );
  list.reloadData();
  return list;
};

/*
 * A controlled poll: pressing an option updates the application's model, then reconfigures
 * the rows. The tally and displayed whole percentages use the same helpers as native polls.
 */
export const pollScreen: Screen = (body) => {
  const { list, toolbar } = makeList(body);
  let poll: PollData = {
    question: 'Where should the next journey go?',
    options: [
      { key: 'mountains', label: 'Mountains', votes: 8 },
      { key: 'coast', label: 'Coast', votes: 5 },
      { key: 'city', label: 'City', votes: 3 },
    ],
  };
  const summary = document.createElement('div');
  summary.className = 'banner';
  list.headerView = summary;
  const update = () => {
    const tally = pollTally(poll.options.map((option) => option.votes));
    summary.textContent = `${poll.question} · ${tally.totalVotes} votes`;
    list.applyChanges();
  };
  list.registerCell(PollCell.IDENTIFIER, () => new PollCell());
  list.dataSource = {
    numberOfItems: () => poll.options.length,
    keyForItem: (_, index) => poll.options[index]!.key,
    contentVersionForItem: (_, index) =>
      `${poll.options[index]!.votes}:${poll.selectedKey ?? ''}`,
    cellForItem: (view, index) => {
      const cell = view.dequeueReusableCell<PollCell>(PollCell.IDENTIFIER);
      cell.onPressItem = (option) => {
        if (poll.selectedKey === option.key) return;
        const previousKey = poll.selectedKey;
        poll = {
          ...poll,
          selectedKey: option.key,
          options: poll.options.map((item) => ({
            ...item,
            votes:
              item.votes +
              Number(item.key === option.key) -
              Number(item.key === previousKey),
          })),
        };
        update();
      };
      cell.configure(
        poll.options[index]!,
        pollTally(poll.options.map((option) => option.votes)).totalVotes,
        poll.selectedKey
      );
      return cell;
    },
  };
  toolbarButton(toolbar, 'Reset votes', () => {
    poll = {
      ...poll,
      selectedKey: undefined,
      options: poll.options.map((option) => ({ ...option, votes: 0 })),
    };
    update();
  });
  update();
  return list;
};

export const SCREENS: Record<string, Screen> = {
  Feed: feedScreen,
  Poll: pollScreen,
  Chat: chatScreen,
  Masonry: masonryScreen,
  Horizontal: horizontalScreen,
  Sticky: stickyScreen,
};
