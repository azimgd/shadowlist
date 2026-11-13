/*
 * The themes import react-native, which does not load under Node. The mock gives them the
 * Platform they read.
 */
jest.mock('react-native', () => ({ Platform: { OS: 'ios' } }), {
  virtual: true,
});

import {
  expectEveryVectorFileReplayed,
  functionOf,
  replayVectors,
} from '../../../../shadowlist-fabric/src/__fixtures__/vectors';
import {
  parseMarkdown,
  parseMarkdownFrom,
  type MarkdownParse,
} from '../assistant/markdown';
import { isSafeUrl } from '../assistant/isSafeUrl';
import { stepsOf } from '../assistant/stepsOf';
import { getUserMessageSizeSpec } from '../assistant/userMessageSizeSpec';
import { boardColumnHeight } from '../board/boardHeights';
import { boardColumnLayout } from '../board/boardWidths';
import { moveBoardItem } from '../board/moveBoardItem';
import type { BoardColumn } from '../board/types';
import { getChatMessageSizeSpec } from '../chat/sizeSpec';
import { groupContactsIntoSections } from '../contacts/contactSections';
import { getSingleImageFrame } from '../feed/singleImageFrame';
import { formatRelativeTime } from '../formatRelativeTime';
import { getPollPercent, getPollTally } from '../poll/pollTally';
import {
  getAvatarColor,
  getInitials,
  getInitialsFontSize,
} from '../primitives/avatarAppearance';
import type { Theme } from '../theme';
import { darkTheme } from '../theme/darkTheme';
import { lightTheme } from '../theme/lightTheme';

/*
 * The vector files of packages/shadowlist-core-tests/vectors, replayed through the functions
 * here. tests_utils_vectors.cpp replays the same files through packages/shadowlist-core.
 * A change of behavior on either side fails its replay. After a change that is meant, on both
 * sides, UPDATE_VECTORS=1 yarn test writes the new outputs. theme.json holds the tokens every
 * platform's theme shares, and the tests of the UIKit and Android templates compare their own
 * themes with it.
 */

interface Metrics {
  spacingXxs: number;
  spacingXs: number;
  spacingSm: number;
  spacingMd: number;
  spacingLg: number;
  bodyFontSize: number;
  bodyLineHeight: number;
  bodyLetterSpacing: number;
  captionLineHeight: number;
}

/*
 * Only the tokens the size specs read. The real themes import react-native.
 */
function themeOf(metrics: Metrics): Theme {
  return {
    spacing: {
      xxs: metrics.spacingXxs,
      xs: metrics.spacingXs,
      sm: metrics.spacingSm,
      md: metrics.spacingMd,
      lg: metrics.spacingLg,
    },
    typography: {
      body: {
        fontSize: metrics.bodyFontSize,
        lineHeight: metrics.bodyLineHeight,
        letterSpacing: metrics.bodyLetterSpacing,
      },
      caption: { lineHeight: metrics.captionLineHeight },
    },
  } as unknown as Theme;
}

/*
 * A size spec without the text, which the C++ leaves to its caller.
 */
function numbersOf(spec: object | null): object | null {
  if (spec === null) {
    return null;
  }
  return { ...spec, text: undefined };
}

interface Card {
  id: string;
}

describe('vectors', () => {
  it('avatar.json', () => {
    replayVectors('avatar.json', ({ input, output }) => {
      const { units, paletteSize } = input as {
        name?: string;
        units?: number[];
        paletteSize: number;
      };
      // A case with half a surrogate pair lists its UTF-16 units, which not every JSON reader takes in a string.
      const name = units
        ? String.fromCharCode(...units)
        : (input as { name: string }).name;
      const palette = Array.from(
        { length: paletteSize },
        (_, index) => `color${index}`
      );
      /*
       * The C++ leaves the upper case to its host, and the file holds the letters as the C++
       * returns them. They stand while their upper case is what getInitials returns.
       */
      const upper = getInitials(name);
      const written = (output as { initials?: string } | null)?.initials;
      return {
        colorIndex: palette.indexOf(String(getAvatarColor(name, palette))),
        initials: written?.toUpperCase() === upper ? written : upper,
      };
    });
  });

  it('avatar-initials.json', () => {
    replayVectors('avatar-initials.json', ({ input }) => ({
      fontSize: getInitialsFontSize((input as { size: number }).size),
    }));
  });

  it('relative-time.json', () => {
    const labels = {
      now: 'now:0',
      minutes: (count: number) => `minutes:${count}`,
      hours: (count: number) => `hours:${count}`,
      days: (count: number) => `days:${count}`,
    };
    replayVectors('relative-time.json', ({ input }) => {
      const { dateMs, nowMs } = input as { dateMs: number; nowMs: number };
      const [unit, count] = formatRelativeTime(dateMs, labels, nowMs).split(
        ':'
      );
      // Anything else is the calendar date, which each platform writes itself.
      return count === undefined
        ? { unit: 'date', count: 0 }
        : { unit, count: Number(count) };
    });
  });

  it('chat-layout.json', () => {
    replayVectors('chat-layout.json', (vector) => {
      const name = functionOf(vector, [
        'chatMessageSizeSpec',
        'assistantUserMessageSizeSpec',
      ]);
      if (name === 'assistantUserMessageSizeSpec') {
        const { hasText, hasAttachments, metrics } = vector.input as {
          hasText: boolean;
          hasAttachments: boolean;
          metrics: Metrics;
        };
        return numbersOf(
          getUserMessageSizeSpec(
            {
              id: 'prompt',
              role: 'user',
              text: hasText ? 'Hello' : '',
              attachments: hasAttachments
                ? [{ id: 'file', kind: 'file', name: 'notes.txt' }]
                : undefined,
            },
            themeOf(metrics)
          )
        );
      }
      const { shape, metrics } = vector.input as {
        shape: {
          hasText: boolean;
          hasImages: boolean;
          isOwn: boolean;
          showsAuthor: boolean;
          hasCaption: boolean;
          failed: boolean;
        };
        metrics: Metrics;
      };
      return numbersOf(
        getChatMessageSizeSpec(
          {
            id: 'message',
            author: { id: 'author', name: 'Ada' },
            isOwn: shape.isOwn,
            text: shape.hasText ? 'Hello' : undefined,
            images: shape.hasImages ? ['image'] : undefined,
            status: shape.failed ? 'failed' : undefined,
          },
          themeOf(metrics),
          { caption: shape.hasCaption, showsAuthor: shape.showsAuthor }
        )
      );
    });
  });

  it('feed-layout.json', () => {
    replayVectors('feed-layout.json', ({ input }) => {
      const { width, height } = input as {
        width: number | null;
        height: number | null;
      };
      return getSingleImageFrame({
        uri: 'image',
        width: width ?? undefined,
        height: height ?? undefined,
      });
    });
  });

  it('poll.json', () => {
    replayVectors('poll.json', ({ input }) => {
      const { votes } = input as { votes: number[] };
      const options = votes.map((count, index) => ({
        id: String(index),
        label: '',
        votes: count,
      }));
      const { totalVotes, leadingId } = getPollTally(options);
      return {
        totalVotes,
        leadingIndex: leadingId === undefined ? -1 : Number(leadingId),
        percents: votes.map((count) => getPollPercent(count, totalVotes)),
      };
    });
  });

  it('board-layout.json', () => {
    replayVectors('board-layout.json', (vector) => {
      const name = functionOf(vector, [
        'boardColumnLayout',
        'boardColumnHeight',
      ]);
      if (name === 'boardColumnHeight') {
        const { columnHeights, minimum, maximum } = vector.input as {
          columnHeights: number[];
          minimum: number;
          maximum: number;
        };
        const columns = columnHeights.map((_, index) => ({
          key: `column${index}`,
          title: '',
          items: [],
        }));
        const contentHeights = Object.fromEntries(
          columnHeights.map((height, index) => [`column${index}`, height])
        );
        return boardColumnHeight(columns, contentHeights, 0, minimum, maximum);
      }
      // A null is a width or a padding the caller leaves out.
      const { boardWidth, columnWidth, sidePadding } = vector.input as {
        boardWidth: number;
        columnWidth: number | null;
        sidePadding: number | null;
      };
      return boardColumnLayout(
        boardWidth,
        columnWidth ?? undefined,
        sidePadding ?? undefined
      );
    });
  });

  it('board-move.json', () => {
    replayVectors('board-move.json', ({ input }) => {
      const move = input as {
        columns: string[][];
        itemKey: string;
        destinationColumn: number;
        destinationKey: string;
        destinationIndex: number;
      };
      const columns: BoardColumn<Card>[] = move.columns.map((keys, index) => ({
        key: `column${index}`,
        title: '',
        items: keys.map((id) => ({ id })),
      }));
      const item = columns
        .flatMap((column) => column.items)
        .find((card) => card.id === move.itemKey) ?? { id: move.itemKey };
      const next = moveBoardItem(columns, {
        item,
        sourceColumnKey: '',
        destinationColumnKey: `column${move.destinationColumn}`,
        destinationKey: move.destinationKey,
        destinationIndex: move.destinationIndex,
      });
      const keys = next.map((column) => column.items.map((card) => card.id));
      if (next === columns) {
        return { plan: null, columns: keys };
      }
      // The plan the C++ returns, read off the board before and after.
      const sourceColumn = move.columns.findIndex((column) =>
        column.includes(move.itemKey)
      );
      return {
        plan: {
          sourceColumn,
          sourceIndex: move.columns[sourceColumn]?.indexOf(move.itemKey) ?? -1,
          destinationColumn: move.destinationColumn,
          destinationIndex: keys[move.destinationColumn]!.indexOf(move.itemKey),
        },
        columns: keys,
      };
    });
  });

  it('markdown.json', () => {
    replayVectors('markdown.json', (vector) => {
      const name = functionOf(vector, ['parseMarkdown', 'isSafeUrl']);
      if (name === 'isSafeUrl') {
        return isSafeUrl((vector.input as { url: string }).url);
      }
      /*
       * The text arrives chunk by chunk, as a streamed reply does. Per chunk the file holds
       * how many blocks the parse kept from the one before and how many it has.
       */
      const { chunks } = vector.input as { chunks: string[] };
      const steps: number[][] = [];
      let parse: MarkdownParse | null = null;
      let text = '';
      for (const chunk of chunks) {
        text += chunk;
        const next = parseMarkdownFrom(parse, text);
        let kept = 0;
        while (
          parse !== null &&
          kept < next.blocks.length &&
          next.blocks[kept] === parse.blocks[kept]
        ) {
          kept++;
        }
        expect(next.blocks).toEqual(parseMarkdown(text));
        steps.push([kept, next.blocks.length]);
        parse = next;
      }
      return { steps, blocks: parseMarkdown(text) };
    });
  });

  it('group-into-sections.json', () => {
    replayVectors('group-into-sections.json', ({ input }) => {
      const { names } = input as { names: string[] };
      const sections = groupContactsIntoSections(
        names.map((name, index) => ({ id: `contact-${index}`, name }))
      );
      return {
        sections: sections.map((section) => ({
          title: section.title,
          names: section.data.map((contact) => contact.name),
        })),
      };
    });
  });

  it('theme.json', () => {
    const themes: Record<string, Theme> = {
      light: lightTheme,
      dark: darkTheme,
    };
    replayVectors('theme.json', ({ input }) => {
      const theme = themes[(input as { scheme: string }).scheme];
      if (theme === undefined) {
        throw new Error('theme.json names a scheme that is not light or dark');
      }
      return {
        colors: theme.colors,
        typography: theme.typography,
        spacing: theme.spacing,
        radius: theme.radius,
        grouped: theme.grouped,
        rowInset: theme.rowInset,
        tapTarget: theme.tapTarget,
      };
    });
  });

  it('assistant-steps.json', () => {
    replayVectors('assistant-steps.json', ({ input }) => {
      const { content, positions } = input as {
        content: string;
        positions: (number | null)[];
      };
      const calls = positions.map((at, index) => ({
        id: String(index),
        name: '',
        input: '',
        status: 'running' as const,
        ...(at === null ? {} : { at }),
      }));
      return stepsOf(content, calls).map((step) =>
        step.kind === 'calls'
          ? {
              kind: 'calls',
              callIndices: step.calls.map((call) => Number(call.id)),
            }
          : // A lone UTF-16 surrogate crosses a native UTF-8 boundary as U+FFFD.
            {
              kind: 'text',
              text: Buffer.from(step.text, 'utf8').toString('utf8'),
              at: step.at,
            }
      );
    });
  });

  it('every file in the folder is replayed', () => {
    expectEveryVectorFileReplayed('shadowlist-utils');
  });
});
