import type { Theme } from '../../theme';
import { BUBBLE_PADDING_HORIZONTAL, getChatMessageSizeSpec } from '../sizeSpec';
import type { ChatMessage } from '../types';

/*
 * Only the tokens the spec reads. The real themes import react-native.
 */
const theme = {
  spacing: { xxs: 2, xs: 4, sm: 8, md: 12 },
  typography: {
    body: { fontSize: 17, lineHeight: 22, letterSpacing: -0.4 },
    caption: { fontSize: 12, lineHeight: 16 },
  },
} as unknown as Theme;

const author = { id: 'a', name: 'Ada' };

function message(overrides: Partial<ChatMessage> = {}): ChatMessage {
  return { id: 'm1', author, isOwn: false, text: 'Hello', ...overrides };
}

describe('getChatMessageSizeSpec', () => {
  it('counts the sender line on a message from someone else', () => {
    const spec = getChatMessageSizeSpec(message(), theme);
    expect(spec?.insetHeight).toBe(2 * 2 + 2 * 8 + 16 + 2);
    expect(spec?.text).toBe('Hello');
  });

  it('leaves the sender line out of a message that hides its author', () => {
    const spec = getChatMessageSizeSpec(message(), theme, {
      showsAuthor: false,
    });
    expect(spec?.insetHeight).toBe(2 * 2 + 2 * 8);
  });

  it("never counts a sender line on the reader's own message", () => {
    const shown = getChatMessageSizeSpec(message({ isOwn: true }), theme);
    const hidden = getChatMessageSizeSpec(message({ isOwn: true }), theme, {
      showsAuthor: false,
    });
    expect(shown?.insetHeight).toBe(2 * 2 + 2 * 8);
    expect(hidden?.insetHeight).toBe(shown?.insetHeight);
  });

  it('adds the caption and the failed line', () => {
    const spec = getChatMessageSizeSpec(
      message({ isOwn: true, status: 'failed' }),
      theme,
      { caption: true }
    );
    expect(spec?.insetHeight).toBe(2 * 2 + 2 * 8 + (16 + 4) + (16 + 4));
  });

  it('takes the bubble padding off the width', () => {
    const spec = getChatMessageSizeSpec(message(), theme);
    expect(spec?.insetWidth).toBe(
      0.75 * 2 * 12 + 2 * BUBBLE_PADDING_HORIZONTAL
    );
  });

  it('measures image and empty messages natively', () => {
    expect(
      getChatMessageSizeSpec(message({ images: ['a.png'] }), theme)
    ).toBeNull();
    expect(getChatMessageSizeSpec(message({ text: '' }), theme)).toBeNull();
  });
});
