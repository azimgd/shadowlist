/*
 * Chat layout tests: the size spec of a chat message and of the user's message in an assistant
 * thread.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/RowSizeSpec.hpp>
#include <shadowlist-core/utils/ChatLayout.hpp>

#include <string>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

ChatMessageShape textMessage() {
  ChatMessageShape shape;
  shape.hasText = true;
  return shape;
}

double insetHeightOf(const ChatMessageShape& shape) {
  RowSizeSpec spec;
  CHECK(chatMessageSizeSpec(shape, ChatBubbleMetrics{}, spec));
  return spec.insetHeight;
}

}

TEST(a_message_from_someone_else_counts_its_sender_line) {
  CHECK_EQ(insetHeightOf(textMessage()), 2.0 * 2.0 + 2.0 * 8.0 + 16.0 + 2.0);
}

TEST(a_message_that_hides_its_author_has_no_sender_line) {
  ChatMessageShape shape = textMessage();
  shape.showsAuthor = false;
  CHECK_EQ(insetHeightOf(shape), 2.0 * 2.0 + 2.0 * 8.0);
}

TEST(the_readers_own_message_never_counts_a_sender_line) {
  ChatMessageShape shown = textMessage();
  shown.isOwn = true;
  ChatMessageShape hidden = shown;
  hidden.showsAuthor = false;
  CHECK_EQ(insetHeightOf(shown), 2.0 * 2.0 + 2.0 * 8.0);
  CHECK_EQ(insetHeightOf(hidden), insetHeightOf(shown));
}

TEST(a_caption_adds_its_line) {
  ChatMessageShape shape = textMessage();
  shape.isOwn = true;
  shape.hasCaption = true;
  CHECK_EQ(insetHeightOf(shape), 2.0 * 2.0 + 2.0 * 8.0 + 16.0 + 4.0);
}

TEST(a_failed_message_adds_its_status_line) {
  ChatMessageShape shape = textMessage();
  shape.isOwn = true;
  shape.failed = true;
  CHECK_EQ(insetHeightOf(shape), 2.0 * 2.0 + 2.0 * 8.0 + 16.0 + 4.0);
}

TEST(a_message_with_images_has_no_size_spec) {
  ChatMessageShape shape = textMessage();
  shape.hasImages = true;
  RowSizeSpec spec;
  spec.insetHeight = 7.0;
  CHECK(!chatMessageSizeSpec(shape, ChatBubbleMetrics{}, spec));
  // The spec is left alone.
  CHECK_EQ(spec.insetHeight, 7.0);
  CHECK(!chatMessageSizeSpec(ChatMessageShape{}, ChatBubbleMetrics{}, spec));
}

TEST(a_chat_size_spec_takes_its_text_style_and_width_from_the_metrics) {
  ChatBubbleMetrics metrics;
  metrics.bodyFontSize = 13.0;
  metrics.bodyLineHeight = 16.0;
  metrics.bodyLetterSpacing = -0.08;
  metrics.spacingMd = 8.0;
  RowSizeSpec spec;
  spec.key = "message";
  spec.text = "Hello";
  CHECK(chatMessageSizeSpec(textMessage(), metrics, spec));
  CHECK_EQ(spec.fontSize, 13.0);
  CHECK_EQ(spec.lineHeight, 16.0);
  CHECK_EQ(spec.letterSpacing, -0.08);
  CHECK_EQ(spec.widthFraction, CHAT_BUBBLE_WIDTH_FRACTION);
  // The bubble's share of the row's padding, then the bubble's own.
  CHECK_EQ(spec.insetWidth, 0.75 * 2.0 * 8.0 + 2.0 * CHAT_BUBBLE_PADDING_HORIZONTAL);
  // The key and the text are the caller's.
  CHECK_EQ(spec.key, std::string("message"));
  CHECK_EQ(spec.text, std::string("Hello"));
}

TEST(the_default_chat_metrics_are_the_phone_theme) {
  ChatBubbleMetrics metrics;
  CHECK_EQ(metrics.spacingXxs, 2.0);
  CHECK_EQ(metrics.spacingXs, 4.0);
  CHECK_EQ(metrics.spacingSm, 8.0);
  CHECK_EQ(metrics.spacingMd, 12.0);
  CHECK_EQ(metrics.spacingLg, 16.0);
  CHECK_EQ(metrics.bodyFontSize, 17.0);
  CHECK_EQ(metrics.bodyLineHeight, 22.0);
  CHECK_EQ(metrics.bodyLetterSpacing, -0.43);
  CHECK_EQ(metrics.captionLineHeight, 16.0);
}

TEST(an_assistant_user_message_is_sized_from_its_bubble) {
  RowSizeSpec spec;
  CHECK(assistantUserMessageSizeSpec(true, false, ChatBubbleMetrics{}, spec));
  CHECK_EQ(spec.widthFraction, ASSISTANT_USER_BUBBLE_WIDTH_FRACTION);
  CHECK_EQ(spec.insetWidth, 0.8 * 16.0 * 2.0 + 14.0 * 2.0);
  CHECK_EQ(spec.insetHeight, 8.0 * 2.0 + 10.0 * 2.0);
  CHECK_EQ(spec.fontSize, 17.0);
}

TEST(an_assistant_user_message_with_attachments_has_no_size_spec) {
  RowSizeSpec spec;
  CHECK(!assistantUserMessageSizeSpec(true, true, ChatBubbleMetrics{}, spec));
  CHECK(!assistantUserMessageSizeSpec(false, false, ChatBubbleMetrics{}, spec));
}
