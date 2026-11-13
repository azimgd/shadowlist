/*
 * The base tokens read the platform, and react-native does not load under Node.
 */
jest.mock('react-native', () => ({ Platform: { OS: 'ios' } }), {
  virtual: true,
});

import { readFileSync } from 'fs';
import { join } from 'path';
import {
  BUBBLE_PADDING_HORIZONTAL as USER_BUBBLE_PADDING_HORIZONTAL,
  BUBBLE_PADDING_VERTICAL as USER_BUBBLE_PADDING_VERTICAL,
  BUBBLE_WIDTH_FRACTION as USER_BUBBLE_WIDTH_FRACTION,
} from '../assistant/userMessageSizeSpec';
import {
  COLUMN_GAP,
  COLUMN_MAX_WIDTH,
  COLUMN_SHARE,
  SIDE_PADDING,
} from '../board/boardWidths';
import {
  BUBBLE_PADDING_HORIZONTAL,
  BUBBLE_WIDTH_FRACTION,
} from '../chat/sizeSpec';
import {
  MAX_ASPECT_RATIO,
  MIN_ASPECT_RATIO,
  SINGLE_IMAGE_HEIGHT,
} from '../feed/singleImageFrame';
import { baseTokens } from '../theme/baseTokens';

/*
 * What the core constant name is set to in a header of packages/shadowlist-core/utils: a
 * number, or a division of two.
 */
function coreConstant(header: string, name: string): number {
  const source = readFileSync(
    join(__dirname, '../../../../shadowlist-core/utils', header),
    'utf8'
  );
  const match = new RegExp(
    `constexpr [\\w:]+ ${name} = ([\\d.]+)(?: / ([\\d.]+))?;`
  ).exec(source);
  if (!match) {
    throw new Error(`${name} is not in ${header}`);
  }
  return match[2] === undefined
    ? Number(match[1])
    : Number(match[1]) / Number(match[2]);
}

/*
 * The default of a number field of the core's ChatBubbleMetrics.
 */
function chatMetric(field: string): number {
  const source = readFileSync(
    join(__dirname, '../../../../shadowlist-core/utils/ChatLayout.hpp'),
    'utf8'
  );
  const match = new RegExp(`double ${field} = (-?[\\d.]+);`).exec(source);
  if (!match) {
    throw new Error(`${field} is not in ChatBubbleMetrics`);
  }
  return Number(match[1]);
}

describe('core constants', () => {
  it('match the phone theme the chat metrics default to', () => {
    const { spacing, typography } = baseTokens;
    expect(chatMetric('spacingXxs')).toBe(spacing.xxs);
    expect(chatMetric('spacingXs')).toBe(spacing.xs);
    expect(chatMetric('spacingSm')).toBe(spacing.sm);
    expect(chatMetric('spacingMd')).toBe(spacing.md);
    expect(chatMetric('spacingLg')).toBe(spacing.lg);
    expect(chatMetric('bodyFontSize')).toBe(typography.body.fontSize);
    expect(chatMetric('bodyLineHeight')).toBe(typography.body.lineHeight);
    expect(chatMetric('bodyLetterSpacing')).toBe(typography.body.letterSpacing);
    expect(chatMetric('captionLineHeight')).toBe(typography.caption.lineHeight);
  });

  it('match the chat bubble numbers', () => {
    expect(BUBBLE_WIDTH_FRACTION).toBe(
      coreConstant('ChatLayout.hpp', 'CHAT_BUBBLE_WIDTH_FRACTION')
    );
    expect(BUBBLE_PADDING_HORIZONTAL).toBe(
      coreConstant('ChatLayout.hpp', 'CHAT_BUBBLE_PADDING_HORIZONTAL')
    );
  });

  it('match the assistant user bubble numbers', () => {
    expect(USER_BUBBLE_WIDTH_FRACTION).toBe(
      coreConstant('ChatLayout.hpp', 'ASSISTANT_USER_BUBBLE_WIDTH_FRACTION')
    );
    expect(USER_BUBBLE_PADDING_HORIZONTAL).toBe(
      coreConstant('ChatLayout.hpp', 'ASSISTANT_USER_BUBBLE_PADDING_HORIZONTAL')
    );
    expect(USER_BUBBLE_PADDING_VERTICAL).toBe(
      coreConstant('ChatLayout.hpp', 'ASSISTANT_USER_BUBBLE_PADDING_VERTICAL')
    );
  });

  it('match the board layout numbers', () => {
    expect(COLUMN_GAP).toBe(
      coreConstant('BoardColumnLayout.hpp', 'BOARD_COLUMN_GAP')
    );
    expect(COLUMN_SHARE).toBe(
      coreConstant('BoardColumnLayout.hpp', 'BOARD_COLUMN_SHARE')
    );
    expect(COLUMN_MAX_WIDTH).toBe(
      coreConstant('BoardColumnLayout.hpp', 'BOARD_COLUMN_MAX_WIDTH')
    );
    expect(SIDE_PADDING).toBe(
      coreConstant('BoardColumnLayout.hpp', 'BOARD_SIDE_PADDING')
    );
  });

  it('match the feed image numbers', () => {
    expect(SINGLE_IMAGE_HEIGHT).toBe(
      coreConstant('FeedImageFrame.hpp', 'FEED_SINGLE_IMAGE_HEIGHT')
    );
    expect(MIN_ASPECT_RATIO).toBe(
      coreConstant('FeedImageFrame.hpp', 'FEED_IMAGE_MIN_ASPECT_RATIO')
    );
    expect(MAX_ASPECT_RATIO).toBe(
      coreConstant('FeedImageFrame.hpp', 'FEED_IMAGE_MAX_ASPECT_RATIO')
    );
  });
});
