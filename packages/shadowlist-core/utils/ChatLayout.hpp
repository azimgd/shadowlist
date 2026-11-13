#pragma once

#include <shadowlist-core/host/RowSizeSpec.hpp>

namespace azimgd::shadowlist {

/*
 * The theme numbers a chat bubble is laid out from, in points. The defaults are the phone
 * theme of the templates.
 */
struct ChatBubbleMetrics {
  double spacingXxs = 2.0;
  double spacingXs = 4.0;
  double spacingSm = 8.0;
  double spacingMd = 12.0;
  double spacingLg = 16.0;
  double bodyFontSize = 17.0;
  double bodyLineHeight = 22.0;
  double bodyLetterSpacing = -0.43;
  double captionLineHeight = 16.0;
};

/*
 * A chat bubble's column takes this share of the row's content box, and the bubble pads its
 * text by this much on each side.
 */
constexpr double CHAT_BUBBLE_WIDTH_FRACTION = 0.75;
constexpr double CHAT_BUBBLE_PADDING_HORIZONTAL = 14.0;

/*
 * The same for the user's bubble in an assistant thread, with its padding above and below.
 */
constexpr double ASSISTANT_USER_BUBBLE_WIDTH_FRACTION = 0.8;
constexpr double ASSISTANT_USER_BUBBLE_PADDING_HORIZONTAL = 14.0;
constexpr double ASSISTANT_USER_BUBBLE_PADDING_VERTICAL = 10.0;

/*
 * What of a chat message changes its row's height. showsAuthor is whether messages from
 * others carry a sender line, and hasCaption whether the bubble holds a caption line.
 */
struct ChatMessageShape {
  bool hasText = false;
  bool hasImages = false;
  bool isOwn = false;
  bool showsAuthor = true;
  bool hasCaption = false;
  bool failed = false;
};

/*
 * The size spec of a text message, without its key and text. It sets the font size, the line
 * height, the letter spacing, the width fraction and both insets, and leaves the rest of spec.
 * Returns false and leaves spec alone for a message the host measures itself: one with images
 * or without text.
 *
 * The row pads by spacingMd on each side and spacingXxs above and below. The bubble pads by
 * CHAT_BUBBLE_PADDING_HORIZONTAL and spacingSm. A sender line, a caption and a failed status
 * are one caption line each, with spacingXxs under the sender and spacingXs over the other two.
 */
inline bool chatMessageSizeSpec(const ChatMessageShape& shape, const ChatBubbleMetrics& metrics, RowSizeSpec& spec) {
  if (!shape.hasText || shape.hasImages) {
    return false;
  }
  double senderHeight = metrics.captionLineHeight + metrics.spacingXxs;
  double captionHeight = metrics.captionLineHeight + metrics.spacingXs;
  double failedHeight = metrics.captionLineHeight + metrics.spacingXs;

  spec.fontSize = metrics.bodyFontSize;
  spec.lineHeight = metrics.bodyLineHeight;
  spec.letterSpacing = metrics.bodyLetterSpacing;
  spec.widthFraction = CHAT_BUBBLE_WIDTH_FRACTION;
  /*
   * The bubble's share of the row's padding, then the bubble's own padding. The product is a
   * statement of its own because a compiler may fuse a product and a sum of one expression,
   * which rounds once where JS rounds twice.
   */
  double rowPadding = CHAT_BUBBLE_WIDTH_FRACTION * 2.0 * metrics.spacingMd;
  spec.insetWidth = rowPadding + 2.0 * CHAT_BUBBLE_PADDING_HORIZONTAL;
  spec.insetHeight = 2.0 * metrics.spacingXxs + 2.0 * metrics.spacingSm +
    (shape.isOwn || !shape.showsAuthor ? 0.0 : senderHeight) + (shape.hasCaption ? captionHeight : 0.0) +
    (shape.failed ? failedHeight : 0.0);
  return true;
}

/*
 * The size spec of the user's message in an assistant thread, the same way. Returns false for
 * a message with attachments or without text. The row pads by spacingLg on each side and
 * spacingSm above and below.
 */
inline bool assistantUserMessageSizeSpec(
  bool hasText,
  bool hasAttachments,
  const ChatBubbleMetrics& metrics,
  RowSizeSpec& spec) {
  if (!hasText || hasAttachments) {
    return false;
  }
  spec.fontSize = metrics.bodyFontSize;
  spec.lineHeight = metrics.bodyLineHeight;
  spec.letterSpacing = metrics.bodyLetterSpacing;
  spec.widthFraction = ASSISTANT_USER_BUBBLE_WIDTH_FRACTION;
  double rowPadding = ASSISTANT_USER_BUBBLE_WIDTH_FRACTION * metrics.spacingLg * 2.0;
  spec.insetWidth = rowPadding + ASSISTANT_USER_BUBBLE_PADDING_HORIZONTAL * 2.0;
  spec.insetHeight = metrics.spacingSm * 2.0 + ASSISTANT_USER_BUBBLE_PADDING_VERTICAL * 2.0;
  return true;
}

}
