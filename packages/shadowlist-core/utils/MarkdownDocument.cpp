#include <shadowlist-core/utils/MarkdownDocument.hpp>

#include <shadowlist-core/utils/Text.hpp>

#include <cstdlib>
#include <utility>

namespace azimgd::shadowlist {

namespace {

constexpr std::string_view FENCE = "```";
constexpr std::size_t MAX_HEADING_LEVEL = 6;
constexpr std::size_t MIN_RULE_LENGTH = 3;

/*
 * A list marker may sit behind up to three white space characters. A nested item then starts
 * its own block instead of joining the paragraph above. Four or more is indented code, left
 * alone.
 */
constexpr std::size_t MAX_MARKER_INDENT = 3;

constexpr std::string_view SAFE_URL_SCHEMES[] = {"http:", "https:", "mailto:"};

bool isWordCharacter(char letter) {
  return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') || (letter >= '0' && letter <= '9') ||
    letter == '_';
}

bool isDigit(char letter) {
  return letter >= '0' && letter <= '9';
}

char lowerAscii(char letter) {
  return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
}

bool hasLineTerminator(std::string_view text) {
  std::size_t offset = 0;
  while (offset < text.size()) {
    if (isLineTerminator(decodeUtf8(text, offset))) {
      return true;
    }
  }
  return false;
}

/*
 * How many times letter repeats at the start of text.
 */
std::size_t leadingRun(std::string_view text, char letter) {
  std::size_t length = 0;
  while (length < text.size() && text[length] == letter) {
    ++length;
  }
  return length;
}

/*
 * What follows a block marker: nothing, or white space and then the block's text, which holds
 * no line terminator. Text after a marker is optional. A line that is only a marker like # or
 * 1. already gets its final block type. Otherwise it is a paragraph for one flush and a heading
 * the next, which remounts the block and moves everything under it.
 */
bool matchMarkerTail(std::string_view rest, std::string_view& text) {
  text = {};
  if (rest.empty()) {
    return true;
  }
  std::string_view tail = trimStart(rest);
  if (tail.size() == rest.size() || hasLineTerminator(tail)) {
    return false;
  }
  text = tail;
  return true;
}

/*
 * The line without its marker indent, or false when the indent is deeper than a marker's.
 */
bool skipMarkerIndent(std::string_view line, std::string_view& rest) {
  std::size_t offset = 0;
  std::size_t indent = 0;
  while (offset < line.size()) {
    std::size_t next = offset;
    if (!isWhiteSpace(decodeUtf8(line, next))) {
      break;
    }
    offset = next;
    ++indent;
  }
  rest = line.substr(offset);
  return indent <= MAX_MARKER_INDENT;
}

/*
 * An opening fence and the word after it. Only white space may follow the word.
 */
bool matchFenceOpen(std::string_view line, std::string_view& language) {
  if (line.substr(0, FENCE.size()) != FENCE) {
    return false;
  }
  std::string_view rest = line.substr(FENCE.size());
  std::size_t offset = 0;
  while (offset < rest.size()) {
    std::size_t next = offset;
    if (isWhiteSpace(decodeUtf8(rest, next))) {
      break;
    }
    offset = next;
  }
  language = rest.substr(0, offset);
  return isBlank(rest.substr(offset));
}

bool matchFenceClose(std::string_view line) {
  return line.substr(0, FENCE.size()) == FENCE && isBlank(line.substr(FENCE.size()));
}

/*
 * One or two backticks alone on a line: a fence that is still streaming in.
 */
bool matchPartialFence(std::string_view line) {
  std::size_t ticks = leadingRun(line, '`');
  return ticks >= 1 && ticks < FENCE.size() && isBlank(line.substr(ticks));
}

bool matchHeading(std::string_view line, int& level, std::string_view& text) {
  std::size_t hashes = leadingRun(line, '#');
  if (hashes == 0 || hashes > MAX_HEADING_LEVEL || !matchMarkerTail(line.substr(hashes), text)) {
    return false;
  }
  level = static_cast<int>(hashes);
  return true;
}

bool matchRule(std::string_view line) {
  std::size_t length = leadingRun(line, '-');
  if (length == 0) {
    length = leadingRun(line, '*');
  }
  return length >= MIN_RULE_LENGTH && isBlank(line.substr(length));
}

/*
 * A quote line and its text, without the one white space character that may follow the mark.
 */
bool matchQuote(std::string_view line, std::string_view& text) {
  if (line.empty() || line[0] != '>') {
    return false;
  }
  std::string_view rest = line.substr(1);
  std::size_t offset = 0;
  if (!rest.empty() && isWhiteSpace(decodeUtf8(rest, offset))) {
    rest = rest.substr(offset);
  }
  if (hasLineTerminator(rest)) {
    return false;
  }
  text = rest;
  return true;
}

/*
 * A bullet line and its text. Bold text at the start of a line must not become a bullet. The
 * marker needs white space after it or must stand alone.
 */
bool matchBullet(std::string_view line, std::string_view& text) {
  std::string_view rest;
  if (!skipMarkerIndent(line, rest) || rest.empty() || (rest[0] != '-' && rest[0] != '*')) {
    return false;
  }
  return matchMarkerTail(rest.substr(1), text);
}

/*
 * A numbered line, its number's digits and its text.
 */
bool matchOrdered(std::string_view line, std::string_view& digits, std::string_view& text) {
  std::string_view rest;
  if (!skipMarkerIndent(line, rest)) {
    return false;
  }
  std::size_t count = 0;
  while (count < rest.size() && isDigit(rest[count])) {
    ++count;
  }
  if (count == 0 || count == rest.size() || rest[count] != '.' || !matchMarkerTail(rest.substr(count + 1), text)) {
    return false;
  }
  digits = rest.substr(0, count);
  return true;
}

bool isTableRow(std::string_view line) {
  return !line.empty() && line[0] == '|';
}

/*
 * A row with no cell content. That is the alignment row, any part of it streamed so far, and
 * the bare pipe every row starts as. Matching only the full alignment row would show these as
 * data rows that vanish a token later. The table would lose a row mid stream.
 */
bool isTableStructureRow(std::string_view line) {
  std::size_t offset = 0;
  while (offset < line.size()) {
    char32_t codePoint = decodeUtf8(line, offset);
    if (!isWhiteSpace(codePoint) && codePoint != ':' && codePoint != '|' && codePoint != '-') {
      return false;
    }
  }
  return true;
}

bool isBlockStart(std::string_view line) {
  int level = 0;
  std::string_view text;
  std::string_view digits;
  return matchFenceOpen(line, text) || matchHeading(line, level, text) || matchRule(line) || matchQuote(line, text) ||
    matchBullet(line, text) || matchOrdered(line, digits, text) || isTableRow(line);
}

std::vector<MarkdownInline> parseInline(std::string_view text) {
  std::vector<MarkdownInline> inlines;
  std::string plain;
  std::size_t index = 0;

  auto flushPlain = [&] {
    if (!plain.empty()) {
      inlines.push_back({MarkdownInlineStyle::Text, plain, "", false});
      plain.clear();
    }
  };

  /*
   * A styled run from after its opener to marker, or to the end of the text without one. An
   * empty run adds nothing.
   */
  auto readStyled = [&](MarkdownInlineStyle style, std::string_view marker, std::size_t from) {
    flushPlain();
    std::size_t end = text.find(marker, from);
    std::string_view body = text.substr(from, end == std::string_view::npos ? end : end - from);
    if (!body.empty()) {
      inlines.push_back({style, std::string(body), "", false});
    }
    index = end == std::string_view::npos ? text.size() : end + marker.size();
  };

  while (index < text.size()) {
    char letter = text[index];
    bool hasNext = index + 1 < text.size();
    char next = hasNext ? text[index + 1] : '\0';

    if (letter == '`') {
      readStyled(MarkdownInlineStyle::Code, "`", index + 1);
      continue;
    }

    if (letter == '*' && hasNext && next == '*') {
      readStyled(MarkdownInlineStyle::Bold, "**", index + 2);
      continue;
    }

    /*
     * Keep 2 * 3 and snake_case literal. Italics need a non space after the opener, and an
     * underscore also needs a word boundary before it.
     */
    bool opensItalic = (letter == '*' || letter == '_') && hasNext && next != ' ' &&
      !(letter == '_' && index > 0 && isWordCharacter(text[index - 1]));
    if (opensItalic) {
      readStyled(MarkdownInlineStyle::Italic, text.substr(index, 1), index + 1);
      continue;
    }

    /*
     * The label must close right before the opening paren. Searching further ahead would let
     * a stray bracket swallow text up to the next real link, and get slow on bracket heavy lines.
     */
    if (letter == '[') {
      std::size_t labelEnd = text.find(']', index + 1);
      if (labelEnd != std::string_view::npos && labelEnd + 1 < text.size() && text[labelEnd + 1] == '(') {
        flushPlain();
        std::size_t urlEnd = text.find(')', labelEnd + 2);
        bool hasHref = urlEnd != std::string_view::npos;
        std::string href = hasHref ? std::string(text.substr(labelEnd + 2, urlEnd - labelEnd - 2)) : "";
        inlines.push_back(
          {MarkdownInlineStyle::Link, std::string(text.substr(index + 1, labelEnd - index - 1)), href, hasHref});
        index = hasHref ? urlEnd + 1 : text.size();
        continue;
      }
    }

    plain.push_back(letter);
    index += 1;
  }

  flushPlain();
  return inlines;
}

/*
 * A table line's cells without the outer pipes, added to block. A row still streaming in just
 * has fewer cells.
 */
void appendTableRow(MarkdownBlock& block, std::string_view line) {
  std::string_view row = trim(line);
  if (!row.empty() && row.front() == '|') {
    row.remove_prefix(1);
  }
  if (!row.empty() && row.back() == '|') {
    row.remove_suffix(1);
  }
  std::size_t cellCount = 0;
  std::size_t start = 0;
  for (;;) {
    std::size_t pipe = row.find('|', start);
    block.runs.push_back(parseInline(trim(row.substr(start, pipe == std::string_view::npos ? pipe : pipe - start))));
    ++cellCount;
    if (pipe == std::string_view::npos) {
      break;
    }
    start = pipe + 1;
  }
  block.rowCellCounts.push_back(cellCount);
}

/*
 * The lines of source from offset from, and where each starts. Splits on Windows line endings
 * too, or every line keeps a stray carriage return and matches nothing.
 */
void splitLines(
  std::string_view source,
  std::size_t from,
  std::vector<std::string_view>& lines,
  std::vector<std::size_t>& starts) {
  std::size_t start = from;
  for (;;) {
    std::size_t newline = source.find('\n', start);
    starts.push_back(start);
    if (newline == std::string_view::npos) {
      lines.push_back(source.substr(start));
      return;
    }
    std::size_t end = newline > start && source[newline - 1] == '\r' ? newline - 1 : newline;
    lines.push_back(source.substr(start, end - start));
    start = newline + 1;
  }
}

/*
 * Lines first to end, end not included, joined by line feeds.
 */
std::string joinLines(const std::vector<std::string_view>& lines, std::size_t first, std::size_t end) {
  std::string joined;
  for (std::size_t index = first; index < end; ++index) {
    if (index > first) {
      joined.push_back('\n');
    }
    joined.append(lines[index]);
  }
  return joined;
}

const char* blockTypeName(MarkdownBlockType type) {
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

}

std::size_t MarkdownDocument::update(std::string_view source) {
  std::size_t kept = 0;
  bool continues = source.size() >= source_.size() && source.substr(0, source_.size()) == source_;
  if (continues) {
    if (source.size() == source_.size()) {
      return blocks_.size();
    }
    /*
     * A block is kept when every line it was decided on is complete in the text parsed before,
     * which is all lines but the last.
     */
    while (kept < blocks_.size() && endLines_[kept] < lastLine_) {
      ++kept;
    }
    source_.append(source.substr(source_.size()));
  } else {
    source_.assign(source);
  }
  std::size_t lineBase = kept > 0 ? endLines_[kept - 1] : 0;
  std::size_t from = kept > 0 ? endOffsets_[kept - 1] : 0;
  blocks_.resize(kept);
  endLines_.resize(kept);
  endOffsets_.resize(kept);

  std::vector<std::string_view> lines;
  std::vector<std::size_t> starts;
  splitLines(source_, from, lines, starts);
  lastLine_ = lineBase + lines.size() - 1;
  /*
   * Drop an opening fence that is still streaming in. Left in, it joins the paragraph above
   * for one flush and leaves when the third backtick lands, moving everything under it. The
   * closing fence gets the same treatment below.
   */
  if (matchPartialFence(lines.back())) {
    lines.pop_back();
  }

  std::size_t index = 0;
  // Each pass adds at most one block. Where the next pass starts is where it ended.
  auto recordEnd = [&] {
    if (blocks_.size() > endLines_.size()) {
      endLines_.push_back(lineBase + index);
      endOffsets_.push_back(index < starts.size() ? starts[index] : source_.size());
    }
  };
  auto addBlock = [&](MarkdownBlockType type, std::size_t first) -> MarkdownBlock& {
    MarkdownBlock block;
    block.type = type;
    block.key = std::to_string(blocks_.size()) + ":" + blockTypeName(type);
    block.raw = joinLines(lines, first, index);
    blocks_.push_back(std::move(block));
    return blocks_.back();
  };

  while (index < lines.size()) {
    recordEnd();
    std::string_view line = lines[index];
    std::size_t first = index;

    if (isBlank(line)) {
      index += 1;
      continue;
    }

    std::string_view text;
    if (matchFenceOpen(line, text)) {
      std::string_view language = text;
      index += 1;
      std::size_t bodyFirst = index;
      while (index < lines.size() && !matchFenceClose(lines[index])) {
        index += 1;
      }
      std::size_t bodyEnd = index;
      bool closed = index < lines.size();
      if (closed) {
        index += 1;
      }
      /*
       * While the block is open, drop trailing blank lines and a half streamed closing fence.
       * Otherwise the block grows a line or two and shrinks when the fence completes, which
       * moves everything below and the scroll offset. Once closed, blank lines are kept.
       */
      while (!closed && bodyEnd > bodyFirst) {
        std::string_view tail = lines[bodyEnd - 1];
        if (!isBlank(tail) && !matchPartialFence(tail)) {
          break;
        }
        bodyEnd -= 1;
      }
      MarkdownBlock& block = addBlock(MarkdownBlockType::Code, first);
      block.language = std::string(language);
      block.code = joinLines(lines, bodyFirst, bodyEnd);
      block.closed = closed;
      continue;
    }

    int level = 0;
    if (matchHeading(line, level, text)) {
      index += 1;
      MarkdownBlock& block = addBlock(MarkdownBlockType::Heading, first);
      block.level = level;
      block.runs.push_back(parseInline(text));
      continue;
    }

    if (matchRule(line)) {
      index += 1;
      addBlock(MarkdownBlockType::Rule, first);
      continue;
    }

    if (matchQuote(line, text)) {
      std::string quoted;
      while (index < lines.size() && matchQuote(lines[index], text)) {
        if (index > first) {
          quoted.push_back(' ');
        }
        quoted.append(text);
        index += 1;
      }
      addBlock(MarkdownBlockType::Quote, first).runs.push_back(parseInline(quoted));
      continue;
    }

    std::string_view digits;
    bool ordered = matchOrdered(line, digits, text);
    if (ordered || matchBullet(line, text)) {
      double start = ordered ? std::strtod(std::string(digits).c_str(), nullptr) : 1.0;
      std::vector<std::vector<MarkdownInline>> items;
      while (index < lines.size() &&
        (ordered ? matchOrdered(lines[index], digits, text) : matchBullet(lines[index], text))) {
        items.push_back(parseInline(text));
        index += 1;
      }
      MarkdownBlock& block = addBlock(MarkdownBlockType::List, first);
      block.ordered = ordered;
      block.start = start;
      block.runs = std::move(items);
      continue;
    }

    if (isTableRow(line)) {
      while (index < lines.size() && isTableRow(lines[index])) {
        index += 1;
      }
      MarkdownBlock& block = addBlock(MarkdownBlockType::Table, first);
      for (std::size_t row = first; row < index; ++row) {
        if (!isTableStructureRow(lines[row])) {
          appendTableRow(block, lines[row]);
        }
      }
      // A table of structure rows only still has its header, one empty cell.
      if (block.rowCellCounts.empty()) {
        appendTableRow(block, "");
      }
      continue;
    }

    index += 1;
    while (index < lines.size() && !isBlank(lines[index]) && !isBlockStart(lines[index])) {
      index += 1;
    }
    MarkdownBlock& block = addBlock(MarkdownBlockType::Paragraph, first);
    block.runs.push_back(parseInline(block.raw));
  }
  recordEnd();

  return kept;
}

std::vector<MarkdownBlock> parseMarkdown(std::string_view source) {
  MarkdownDocument document;
  document.update(source);
  return document.getBlocks();
}

bool isSafeUrl(std::string_view url) {
  std::string_view trimmed = trim(url);
  for (std::string_view scheme : SAFE_URL_SCHEMES) {
    if (trimmed.size() < scheme.size()) {
      continue;
    }
    bool matches = true;
    for (std::size_t index = 0; index < scheme.size() && matches; ++index) {
      matches = lowerAscii(trimmed[index]) == scheme[index];
    }
    if (matches) {
      return true;
    }
  }
  return false;
}

}
