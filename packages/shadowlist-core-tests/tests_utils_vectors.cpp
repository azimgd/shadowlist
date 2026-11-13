/*
 * Vector tests: every file in vectors/ replayed through the functions of
 * packages/shadowlist-core/utils, and through the board move and mounted range functions of
 * host/. The jest tests of shadowlist-utils and shadowlist replay the same files through the
 * TypeScript, which keeps the two sides from drifting apart. A file in the folder without a
 * VECTOR_TEST here fails every_vector_file_has_a_replay.
 */

#include "TestFramework.hpp"
#include "VectorHelpers.hpp"

#include <shadowlist-core/host/DragBoard.hpp>
#include <shadowlist-core/host/MountedRange.hpp>
#include <shadowlist-core/host/RowSizeSpec.hpp>
#include <shadowlist-core/utils/Avatar.hpp>
#include <shadowlist-core/utils/BoardColumnLayout.hpp>
#include <shadowlist-core/utils/ChatLayout.hpp>
#include <shadowlist-core/utils/ContactSection.hpp>
#include <shadowlist-core/utils/FeedImageFrame.hpp>
#include <shadowlist-core/utils/MarkdownDocument.hpp>
#include <shadowlist-core/utils/PollTally.hpp>
#include <shadowlist-core/utils/RelativeTime.hpp>
#include <shadowlist-core/utils/Text.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

ChatBubbleMetrics metricsOf(const Json& metrics) {
  ChatBubbleMetrics read;
  read.spacingXxs = metrics["spacingXxs"].number;
  read.spacingXs = metrics["spacingXs"].number;
  read.spacingSm = metrics["spacingSm"].number;
  read.spacingMd = metrics["spacingMd"].number;
  read.spacingLg = metrics["spacingLg"].number;
  read.bodyFontSize = metrics["bodyFontSize"].number;
  read.bodyLineHeight = metrics["bodyLineHeight"].number;
  read.bodyLetterSpacing = metrics["bodyLetterSpacing"].number;
  read.captionLineHeight = metrics["captionLineHeight"].number;
  return read;
}

/*
 * A size spec against a case's output, which is null for a row the host measures.
 */
void checkSizeSpec(bool hasSpec, const RowSizeSpec& spec, const Json& output) {
  CHECK_EQ(hasSpec, !output.isNull());
  if (!hasSpec) {
    return;
  }
  CHECK_EQ(spec.fontSize, output["fontSize"].number);
  CHECK_EQ(spec.lineHeight, output["lineHeight"].number);
  CHECK_EQ(spec.letterSpacing, output["letterSpacing"].number);
  CHECK_EQ(spec.widthFraction, output["widthFraction"].number);
  CHECK_EQ(spec.insetWidth, output["insetWidth"].number);
  CHECK_EQ(spec.insetHeight, output["insetHeight"].number);
}

const char* unitName(RelativeTimeUnit unit) {
  switch (unit) {
    case RelativeTimeUnit::Now:
      return "now";
    case RelativeTimeUnit::Minutes:
      return "minutes";
    case RelativeTimeUnit::Hours:
      return "hours";
    case RelativeTimeUnit::Days:
      return "days";
    case RelativeTimeUnit::Date:
      return "date";
  }
  return "";
}

MountedRange rangeOf(const Json& range) {
  return {range["low"].asIndex(), range["high"].asIndex()};
}

void checkRange(const MountedRange& range, const Json& output) {
  CHECK_EQ(indexToNumber(range.low), output["low"].number);
  CHECK_EQ(indexToNumber(range.high), output["high"].number);
}

std::vector<std::vector<std::string>> columnsOf(const Json& columns) {
  std::vector<std::vector<std::string>> read;
  for (const Json& column : columns.items) {
    read.emplace_back();
    for (const Json& key : column.items) {
      read.back().push_back(key.text);
    }
  }
  return read;
}

/*
 * The board after a host carried out plan: the card out of every column, then into its place.
 */
std::vector<std::vector<std::string>> applyBoardMove(
  std::vector<std::vector<std::string>> columns,
  const std::string& itemKey,
  const BoardMovePlan& plan) {
  if (plan.isEmpty()) {
    return columns;
  }
  for (std::vector<std::string>& column : columns) {
    column.erase(std::remove(column.begin(), column.end(), itemKey), column.end());
  }
  std::vector<std::string>& destination = columns[plan.destinationColumn];
  destination.insert(destination.begin() + static_cast<std::ptrdiff_t>(plan.destinationIndex), itemKey);
  return columns;
}

const char* styleName(MarkdownInlineStyle style) {
  switch (style) {
    case MarkdownInlineStyle::Text:
      return "text";
    case MarkdownInlineStyle::Bold:
      return "bold";
    case MarkdownInlineStyle::Italic:
      return "italic";
    case MarkdownInlineStyle::Code:
      return "code";
    case MarkdownInlineStyle::Link:
      return "link";
  }
  return "";
}

const char* typeName(MarkdownBlockType type) {
  switch (type) {
    case MarkdownBlockType::Heading:
      return "heading";
    case MarkdownBlockType::Paragraph:
      return "paragraph";
    case MarkdownBlockType::Quote:
      return "quote";
    case MarkdownBlockType::List:
      return "list";
    case MarkdownBlockType::Code:
      return "code";
    case MarkdownBlockType::Table:
      return "table";
    case MarkdownBlockType::Rule:
      return "rule";
  }
  return "";
}

void checkInlines(const std::vector<MarkdownInline>& inlines, const Json& output) {
  CHECK_EQ(inlines.size(), output.size());
  for (std::size_t index = 0; index < inlines.size(); ++index) {
    const Json& expected = output[index];
    CHECK_EQ(std::string(styleName(inlines[index].style)), expected["style"].text);
    CHECK_EQ(inlines[index].text, expected["text"].text);
    // JS leaves href out of a run without a target.
    CHECK_EQ(inlines[index].hasHref, expected.has("href"));
    if (inlines[index].hasHref) {
      CHECK_EQ(inlines[index].href, expected["href"].text);
    }
  }
}

void checkBlock(const MarkdownBlock& block, const Json& output) {
  CHECK_EQ(std::string(typeName(block.type)), output["type"].text);
  CHECK_EQ(block.key, output["key"].text);
  CHECK_EQ(block.raw, output["raw"].text);
  switch (block.type) {
    case MarkdownBlockType::Heading:
      CHECK_EQ(block.level, static_cast<int>(output["level"].number));
      [[fallthrough]];
    case MarkdownBlockType::Paragraph:
    case MarkdownBlockType::Quote:
      CHECK_EQ(block.runs.size(), std::size_t(1));
      checkInlines(block.runs[0], output["inlines"]);
      break;
    case MarkdownBlockType::List: {
      CHECK_EQ(block.ordered, output["ordered"].boolean);
      CHECK_EQ(block.start, output["start"].number);
      const Json& items = output["items"];
      CHECK_EQ(block.runs.size(), items.size());
      for (std::size_t index = 0; index < items.size(); ++index) {
        checkInlines(block.runs[index], items[index]);
      }
      break;
    }
    case MarkdownBlockType::Code:
      CHECK_EQ(block.language, output["language"].text);
      CHECK_EQ(block.code, output["code"].text);
      CHECK_EQ(block.closed, output["closed"].boolean);
      break;
    case MarkdownBlockType::Table: {
      // JS keeps the header's cells and each row's cells apart. Here they follow one another.
      std::vector<const Json*> lines = {&output["header"]};
      for (const Json& row : output["rows"].items) {
        lines.push_back(&row);
      }
      CHECK_EQ(block.rowCellCounts.size(), lines.size());
      std::size_t cell = 0;
      for (std::size_t line = 0; line < lines.size(); ++line) {
        CHECK_EQ(block.rowCellCounts[line], lines[line]->size());
        for (std::size_t index = 0; index < lines[line]->size(); ++index) {
          checkInlines(block.runs[cell], (*lines[line])[index]);
          ++cell;
        }
      }
      CHECK_EQ(block.runs.size(), cell);
      break;
    }
    case MarkdownBlockType::Rule:
      break;
  }
}

/*
 * The name of an avatar case. A case with half a surrogate pair lists its UTF-16 units, which
 * not every JSON reader takes in a string. Each unit is written as its own sequence, the way a
 * host that copies units one by one hands them over.
 */
std::string avatarNameOf(const Json& input) {
  if (!input.has("units")) {
    return input["name"].text;
  }
  std::string name;
  const Json& units = input["units"];
  for (std::size_t index = 0; index < units.size(); ++index) {
    char32_t unit = static_cast<char32_t>(units[index].number);
    if (unit < 0xD800 || unit > 0xDFFF) {
      appendUtf8(name, unit);
      continue;
    }
    name.push_back(static_cast<char>(0xE0 | (unit >> 12)));
    name.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3F)));
    name.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
  }
  return name;
}

}

VECTOR_TEST(avatar_vectors_replay, "avatar.json") {
  const Json& input = vector["input"];
  const Json& output = vector["output"];
  std::size_t paletteSize = static_cast<std::size_t>(input["paletteSize"].number);
  std::string name = avatarNameOf(input);
  CHECK_EQ(indexToNumber(avatarColorIndex(name, paletteSize)), output["colorIndex"].number);
  CHECK_EQ(avatarInitials(name), output["initials"].text);
}

VECTOR_TEST(avatar_initials_vectors_replay, "avatar-initials.json") {
  AvatarInitialsMetrics metrics = avatarInitialsMetrics(vector["input"]["size"].number);
  CHECK_EQ(metrics.fontSize, vector["output"]["fontSize"].number);
}

VECTOR_TEST(relative_time_vectors_replay, "relative-time.json") {
  RelativeTime time = relativeTime(vector["input"]["dateMs"].number, vector["input"]["nowMs"].number);
  CHECK_EQ(std::string(unitName(time.unit)), vector["output"]["unit"].text);
  CHECK_EQ(static_cast<double>(time.count), vector["output"]["count"].number);
}

VECTOR_TEST(chat_layout_vectors_replay, "chat-layout.json") {
  const Json& input = vector["input"];
  ChatBubbleMetrics metrics = metricsOf(input["metrics"]);
  RowSizeSpec spec;
  if (isVectorFor(vector, "assistantUserMessageSizeSpec")) {
    bool hasSpec =
      assistantUserMessageSizeSpec(input["hasText"].boolean, input["hasAttachments"].boolean, metrics, spec);
    checkSizeSpec(hasSpec, spec, vector["output"]);
    return;
  }
  CHECK(isVectorFor(vector, "chatMessageSizeSpec"));
  const Json& read = input["shape"];
  ChatMessageShape shape;
  shape.hasText = read["hasText"].boolean;
  shape.hasImages = read["hasImages"].boolean;
  shape.isOwn = read["isOwn"].boolean;
  shape.showsAuthor = read["showsAuthor"].boolean;
  shape.hasCaption = read["hasCaption"].boolean;
  shape.failed = read["failed"].boolean;
  checkSizeSpec(chatMessageSizeSpec(shape, metrics, spec), spec, vector["output"]);
}

VECTOR_TEST(feed_layout_vectors_replay, "feed-layout.json") {
  const Json& input = vector["input"];
  const Json& output = vector["output"];
  // A null is a side the image does not give, which the hosts pass as 0.
  double width = input["width"].isNull() ? 0.0 : input["width"].number;
  double height = input["height"].isNull() ? 0.0 : input["height"].number;
  FeedImageFrame frame = feedSingleImageFrame(width, height);
  CHECK_EQ(frame.aspectRatio, output.has("aspectRatio") ? output["aspectRatio"].number : 0.0);
  CHECK_EQ(frame.height, output.has("height") ? output["height"].number : 0.0);
}

VECTOR_TEST(poll_vectors_replay, "poll.json") {
  const Json& output = vector["output"];
  std::vector<double> votes;
  for (const Json& count : vector["input"]["votes"].items) {
    votes.push_back(count.number);
  }
  PollTally tally = pollTally(votes);
  CHECK_EQ(tally.totalVotes, output["totalVotes"].number);
  CHECK_EQ(indexToNumber(tally.leadingIndex), output["leadingIndex"].number);
  CHECK_EQ(votes.size(), output["percents"].size());
  for (std::size_t index = 0; index < votes.size(); ++index) {
    CHECK_EQ(static_cast<double>(pollPercent(votes[index], tally.totalVotes)), output["percents"][index].number);
  }
}

VECTOR_TEST(board_layout_vectors_replay, "board-layout.json") {
  const Json& input = vector["input"];
  const Json& output = vector["output"];
  if (isVectorFor(vector, "boardColumnHeight")) {
    std::vector<double> heights;
    for (const Json& height : input["columnHeights"].items) {
      heights.push_back(height.number);
    }
    CHECK_EQ(boardColumnHeight(heights, input["minimum"].number, input["maximum"].number), output.number);
    return;
  }
  CHECK(isVectorFor(vector, "boardColumnLayout"));
  BoardColumnLayout layout = boardColumnLayout(
    input["boardWidth"].number,
    input["columnWidth"].asOptionalNumber(),
    input["sidePadding"].asOptionalNumber());
  CHECK_EQ(layout.columnWidth, output["columnWidth"].number);
  CHECK_EQ(layout.sidePadding, output["sidePadding"].number);
  CHECK_EQ(layout.snapInterval, output["snapInterval"].number);
}

VECTOR_TEST(board_move_vectors_replay, "board-move.json") {
  const Json& input = vector["input"];
  const Json& output = vector["output"];
  std::vector<std::vector<std::string>> columns = columnsOf(input["columns"]);
  BoardMovePlan plan = planBoardMove(
    columns,
    input["itemKey"].text,
    input["destinationColumn"].asIndex(),
    input["destinationKey"].text,
    input["destinationIndex"].asIndex());
  CHECK_EQ(plan.isEmpty(), output["plan"].isNull());
  if (!plan.isEmpty()) {
    CHECK_EQ(indexToNumber(plan.sourceColumn), output["plan"]["sourceColumn"].number);
    CHECK_EQ(indexToNumber(plan.sourceIndex), output["plan"]["sourceIndex"].number);
    CHECK_EQ(indexToNumber(plan.destinationColumn), output["plan"]["destinationColumn"].number);
    CHECK_EQ(indexToNumber(plan.destinationIndex), output["plan"]["destinationIndex"].number);
  }
  CHECK(applyBoardMove(columns, input["itemKey"].text, plan) == columnsOf(output["columns"]));
}

VECTOR_TEST(mounted_range_vectors_replay, "mounted-range.json") {
  const Json& input = vector["input"];
  const Json& output = vector["output"];
  if (isVectorFor(vector, "initialMountedRange")) {
    checkRange(
      initialMountedRange(
        input["size"].asIndex(),
        input["initialCount"].asIndex(),
        input["inverted"].boolean,
        input["target"].asIndex(),
        input["overscanRows"].asIndex(),
        input["viewPosition"].number),
      output);
  } else if (isVectorFor(vector, "grownMountedRange")) {
    checkRange(
      grownMountedRange(
        rangeOf(input["edges"]),
        input["lowAtStart"].boolean,
        input["highAtEnd"].boolean,
        input["size"].asIndex(),
        input["overscanRowsLeading"].asIndex(),
        input["followTail"].boolean),
      output);
  } else if (isVectorFor(vector, "unionRangeIndices")) {
    std::vector<std::size_t> indices = unionRangeIndices(rangeOf(input["first"]), rangeOf(input["second"]));
    CHECK_EQ(indices.size(), output.size());
    for (std::size_t index = 0; index < indices.size(); ++index) {
      CHECK_EQ(indexToNumber(indices[index]), output[index].number);
    }
  } else {
    CHECK(isVectorFor(vector, "shouldReseedFromIndex"));
    CHECK_EQ(shouldReseedFromIndex(input["previousIndex"].asIndex(), input["nextIndex"].asIndex()), output.boolean);
  }
}

VECTOR_TEST(group_into_sections_vectors_replay, "group-into-sections.json") {
  const std::vector<Json>& names = vector["input"]["names"].items;
  std::vector<std::string> titles;
  for (const Json& name : names) {
    titles.push_back(sectionInitial(name.text));
  }
  std::vector<ContactSection> sections = groupContactsIntoSections(titles);
  const std::vector<Json>& expected = vector["output"]["sections"].items;
  CHECK_EQ(sections.size(), expected.size());
  for (std::size_t index = 0; index < sections.size(); ++index) {
    CHECK_EQ(sections[index].title, expected[index]["title"].text);
    std::vector<std::string> sectionNames;
    for (std::size_t item : sections[index].items) {
      sectionNames.push_back(names[item].text);
    }
    std::vector<std::string> expectedNames;
    for (const Json& name : expected[index]["names"].items) {
      expectedNames.push_back(name.text);
    }
    CHECK(sectionNames == expectedNames);
  }
}

VECTOR_TEST(markdown_vectors_replay, "markdown.json") {
  const Json& input = vector["input"];
  const Json& output = vector["output"];
  if (isVectorFor(vector, "isSafeUrl")) {
    CHECK_EQ(isSafeUrl(input["url"].text), output.boolean);
    return;
  }
  CHECK(isVectorFor(vector, "parseMarkdown"));
  /*
   * The text arrives chunk by chunk, as a streamed reply does. Per chunk the file holds how
   * many blocks the parse kept from the one before and how many it has.
   */
  MarkdownDocument document;
  std::string text;
  const Json& chunks = input["chunks"];
  CHECK_EQ(chunks.size(), output["steps"].size());
  for (std::size_t step = 0; step < chunks.size(); ++step) {
    text += chunks[step].text;
    std::size_t kept = document.update(text);
    CHECK_EQ(indexToNumber(kept), output["steps"][step][0].number);
    CHECK_EQ(indexToNumber(document.getBlocks().size()), output["steps"][step][1].number);
  }
  const std::vector<MarkdownBlock>& blocks = document.getBlocks();
  CHECK_EQ(blocks.size(), output["blocks"].size());
  for (std::size_t index = 0; index < blocks.size(); ++index) {
    checkBlock(blocks[index], output["blocks"][index]);
  }
  CHECK(parseMarkdown(text) == blocks);
}

/*
 * theme.json holds the tokens of the TypeScript themes, which the UIKit and Android templates
 * compare their own themes with. The core holds the ones a chat bubble's size reads, as the
 * defaults of ChatBubbleMetrics.
 */
VECTOR_TEST(theme_vectors_hold_the_chat_bubble_metrics, "theme.json") {
  const Json& output = vector["output"];
  ChatBubbleMetrics metrics;
  CHECK_EQ(metrics.spacingXxs, output["spacing"]["xxs"].number);
  CHECK_EQ(metrics.spacingXs, output["spacing"]["xs"].number);
  CHECK_EQ(metrics.spacingSm, output["spacing"]["sm"].number);
  CHECK_EQ(metrics.spacingMd, output["spacing"]["md"].number);
  CHECK_EQ(metrics.spacingLg, output["spacing"]["lg"].number);
  CHECK_EQ(metrics.bodyFontSize, output["typography"]["body"]["fontSize"].number);
  CHECK_EQ(metrics.bodyLineHeight, output["typography"]["body"]["lineHeight"].number);
  CHECK_EQ(metrics.bodyLetterSpacing, output["typography"]["body"]["letterSpacing"].number);
  CHECK_EQ(metrics.captionLineHeight, output["typography"]["caption"]["lineHeight"].number);
  // The colors and the other tokens are the platforms' to check.
  output.readAll();
}

TEST(every_vector_file_has_a_replay) {
  for (const std::string& file : vectorFilesInFolder()) {
    if (replayedVectorFiles().count(file) == 0) {
      fail("vectors/" + file + " has no VECTOR_TEST");
    }
  }
}
