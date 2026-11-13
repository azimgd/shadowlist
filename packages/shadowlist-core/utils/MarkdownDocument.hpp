#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace azimgd::shadowlist {

/*
 * A small Markdown reader for streamed replies. It covers what chat models write: headings,
 * lists, quotes, code, tables, rules and four inline styles. It copes with text that stops mid
 * token, because during a stream every pass sees an unfinished document.
 *
 * A document is a flat list of blocks. Every block but the last is final. A renderer keeps a
 * block's view while its raw text stays the same, and only the last block changes per flush.
 */

enum class MarkdownInlineStyle { Text, Bold, Italic, Code, Link };

/*
 * A run of one style. An opener with no closer yet runs to the end of its text. Half streamed
 * bold text shows bold right away instead of flashing raw asterisks. A link carries its target
 * in href, and hasHref is false while the target is still streaming in.
 */
struct MarkdownInline {
  MarkdownInlineStyle style = MarkdownInlineStyle::Text;
  std::string text;
  std::string href;
  bool hasHref = false;

  bool operator==(const MarkdownInline& other) const {
    return style == other.style && text == other.text && href == other.href && hasHref == other.hasHref;
  }

  bool operator!=(const MarkdownInline& other) const {
    return !(*this == other);
  }
};

enum class MarkdownBlockType { Heading, Paragraph, Quote, List, Code, Table, Rule };

/*
 * One block. key is the block's index and type, "2:list", and stays the same while the block
 * keeps both. raw is the source lines the block was read from, joined by line feeds.
 */
struct MarkdownBlock {
  MarkdownBlockType type = MarkdownBlockType::Paragraph;
  std::string key;
  std::string raw;

  /*
   * Heading, Paragraph and Quote: one run list. List: one run list per item. Table: one per
   * cell, the header's cells first and then each row's.
   */
  std::vector<std::vector<MarkdownInline>> runs;

  /*
   * Table: how many cells of runs each line holds, the header first. A row still streaming in
   * has fewer cells than the header.
   */
  std::vector<std::size_t> rowCellCounts;

  /*
   * Heading: 1 to 6.
   */
  int level = 0;

  /*
   * List: whether the items are numbered, and the first number. start is a double because the
   * digits are read as a JS number is, whatever their count.
   */
  bool ordered = false;
  double start = 1.0;

  /*
   * Code: the word after the opening fence, the lines between the fences, and whether the
   * closing fence arrived.
   */
  std::string language;
  std::string code;
  bool closed = true;

  bool operator==(const MarkdownBlock& other) const {
    return type == other.type && key == other.key && raw == other.raw && runs == other.runs &&
      rowCellCounts == other.rowCellCounts && level == other.level && ordered == other.ordered &&
      start == other.start && language == other.language && code == other.code && closed == other.closed;
  }

  bool operator!=(const MarkdownBlock& other) const {
    return !(*this == other);
  }
};

/*
 * A parsed document that a longer version of the same text continues from.
 */
class MarkdownDocument final {
public:
  /*
   * Parses source. When source starts with the text parsed before, like a streamed reply, the
   * blocks decided on complete lines are kept and parsing resumes after them. A flush then
   * costs the tail of the reply and gives the same blocks as a parse of the whole. Returns the
   * index of the first block parsed again, which is getBlocks().size() when source is the text
   * parsed before.
   */
  std::size_t update(std::string_view source);

  const std::vector<MarkdownBlock>& getBlocks() const {
    return blocks_;
  }

private:
  std::string source_;
  std::vector<MarkdownBlock> blocks_;

  /*
   * Per block, the line right after it, where the parser looked last to end it, and where that
   * line starts in source_. lastLine_ is the last line of source_, the one that may be unfinished.
   */
  std::vector<std::size_t> endLines_;
  std::vector<std::size_t> endOffsets_;
  std::size_t lastLine_ = 0;
};

/*
 * The blocks of source, parsed in one go.
 */
std::vector<MarkdownBlock> parseMarkdown(std::string_view source);

/*
 * Whether a link may open: http, https and mailto only, in any letter case. Links in a reply
 * come from model output, and a phone, message or deep link could start an action the reader
 * never asked for.
 */
bool isSafeUrl(std::string_view url);

}
