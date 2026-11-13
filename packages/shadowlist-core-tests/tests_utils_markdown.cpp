/*
 * Markdown tests: the blocks and runs of a document, a streamed document parsed step by step,
 * and which links may open.
 */

#include "FuzzHelpers.hpp"
#include "TestFramework.hpp"

#include <shadowlist-core/utils/MarkdownDocument.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

constexpr std::uint32_t MARKDOWN_FUZZ_SEEDS = 100;

/*
 * The documents the streamed parse tests step through: every block type, Windows line endings,
 * a fence that never closes and markers without their text.
 */
const std::vector<std::string>& streamedDocuments() {
  static const std::vector<std::string> documents = {
    "# Plan\n\n1. **Book** [flight](https://x.io)\n2. Pack\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```js\nx\n\n```\n"
    "> quote\n> more\n\n---\nText *it* `code`\n- a\n- b\n# Next\npara\n#\n##x\n",
    "para one\r\nstill one\r\n\r\n- item\r\n  - nested\r\n```\r\ncode\r\n``\r\n```\r\nafter",
    "```\nopen fence never closed\n\n\n",
    "a\n#\n#b\n# c\n***\n**bold** start\n|x|\n|-|\n",
    "| a | b |\r\n|---|---|\r\n| 1 | 2 |\r\n\r\ncaf\xC3\xA9 \xF0\x9F\x98\x80 _\xC3\xA9t\xC3\xA9_\r\n",
  };
  return documents;
}

std::vector<MarkdownBlockType> typesOf(const std::vector<MarkdownBlock>& blocks) {
  std::vector<MarkdownBlockType> types;
  for (const MarkdownBlock& block : blocks) {
    types.push_back(block.type);
  }
  return types;
}

/*
 * The runs of a document that is one paragraph.
 */
std::vector<MarkdownInline> runsOf(std::string_view source) {
  std::vector<MarkdownBlock> blocks = parseMarkdown(source);
  CHECK_EQ(blocks.size(), std::size_t(1));
  CHECK(blocks[0].type == MarkdownBlockType::Paragraph);
  return blocks[0].runs[0];
}

MarkdownInline run(MarkdownInlineStyle style, std::string text) {
  return {style, std::move(text), "", false};
}

MarkdownInline link(std::string text, std::string href) {
  return {MarkdownInlineStyle::Link, std::move(text), std::move(href), true};
}

/*
 * Feeds source to a document cut by cut and checks every step against a parse of the whole
 * prefix. The blocks a step says it kept must be the ones the step before had.
 */
void checkStreamedParse(const std::string& source, const std::vector<std::size_t>& cuts) {
  MarkdownDocument document;
  std::vector<MarkdownBlock> previous;
  for (std::size_t cut : cuts) {
    std::string_view prefix = std::string_view(source).substr(0, cut);
    std::size_t kept = document.update(prefix);
    const std::vector<MarkdownBlock>& blocks = document.getBlocks();
    if (blocks != parseMarkdown(prefix)) {
      fail("a streamed parse differs from a full parse at byte " + std::to_string(cut));
    }
    if (kept > previous.size() || kept > blocks.size()) {
      fail("a streamed parse kept more blocks than it had at byte " + std::to_string(cut));
    }
    for (std::size_t index = 0; index < kept; ++index) {
      if (blocks[index] != previous[index]) {
        fail("a kept block changed at byte " + std::to_string(cut));
      }
    }
    for (std::size_t index = 0; index < blocks.size(); ++index) {
      if (blocks[index].key.rfind(std::to_string(index) + ":", 0) != 0) {
        fail("a block key does not start with its index at byte " + std::to_string(cut));
      }
    }
    previous = blocks;
  }
}

}

TEST(markdown_parses_headings_paragraphs_and_lists) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("# Title\n\nSome **bold** text\n\n- one\n- two");
  std::vector<MarkdownBlockType> expected = {
    MarkdownBlockType::Heading, MarkdownBlockType::Paragraph, MarkdownBlockType::List};
  CHECK(typesOf(blocks) == expected);
  CHECK_EQ(blocks[0].level, 1);
  CHECK_EQ(blocks[0].key, std::string("0:heading"));
  CHECK_EQ(blocks[1].key, std::string("1:paragraph"));
  CHECK_EQ(blocks[1].raw, std::string("Some **bold** text"));
  CHECK_EQ(blocks[2].key, std::string("2:list"));
  CHECK_EQ(blocks[2].raw, std::string("- one\n- two"));
  CHECK_EQ(blocks[2].runs.size(), std::size_t(2));
  CHECK(!blocks[2].ordered);
}

TEST(an_unterminated_code_fence_is_open_while_streaming) {
  std::vector<MarkdownBlock> open = parseMarkdown("```ts\nconst a = 1;");
  CHECK(open[0].type == MarkdownBlockType::Code);
  CHECK_EQ(open[0].language, std::string("ts"));
  CHECK(!open[0].closed);

  std::vector<MarkdownBlock> closed = parseMarkdown("```ts\nconst a = 1;\n```");
  CHECK(closed[0].closed);
  CHECK_EQ(closed[0].code, std::string("const a = 1;"));
}

TEST(an_open_code_fence_drops_trailing_blank_lines_and_a_half_closing_fence) {
  CHECK_EQ(parseMarkdown("```\ncode\n\n\n")[0].code, std::string("code"));
  CHECK_EQ(parseMarkdown("```\ncode\n``")[0].code, std::string("code"));
  // Once closed, blank lines are kept.
  CHECK_EQ(parseMarkdown("```\n\ncode\n\n```")[0].code, std::string("\ncode\n"));
}

TEST(a_half_fence_on_the_last_line_is_dropped) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("text\n``");
  CHECK_EQ(blocks.size(), std::size_t(1));
  CHECK_EQ(blocks[0].raw, std::string("text"));
  // In the middle it is text like any other.
  CHECK_EQ(parseMarkdown("text\n``\nmore")[0].raw, std::string("text\n``\nmore"));
}

TEST(a_line_that_is_only_a_marker_already_has_its_block_type) {
  CHECK(parseMarkdown("#")[0].type == MarkdownBlockType::Heading);
  CHECK(parseMarkdown("-")[0].type == MarkdownBlockType::List);
  CHECK(parseMarkdown("1.")[0].type == MarkdownBlockType::List);
  CHECK(parseMarkdown("|")[0].type == MarkdownBlockType::Table);
  // A marker needs white space before its text.
  CHECK(parseMarkdown("#tag")[0].type == MarkdownBlockType::Paragraph);
  CHECK(parseMarkdown("1.5")[0].type == MarkdownBlockType::Paragraph);
  CHECK(parseMarkdown("####### seven")[0].type == MarkdownBlockType::Paragraph);
}

TEST(bold_text_at_the_start_of_a_line_is_not_a_bullet) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("**bold** start");
  CHECK(blocks[0].type == MarkdownBlockType::Paragraph);
  CHECK(blocks[0].runs[0][0] == run(MarkdownInlineStyle::Bold, "bold"));
}

TEST(a_rule_is_three_or_more_dashes_or_stars) {
  std::vector<MarkdownBlockType> expected = {MarkdownBlockType::Rule, MarkdownBlockType::Rule,
    MarkdownBlockType::Paragraph};
  CHECK(typesOf(parseMarkdown("---\n*****  \n--")) == expected);
}

TEST(quote_lines_join_into_one_block) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("> first\n>second\nplain");
  CHECK_EQ(blocks.size(), std::size_t(2));
  CHECK(blocks[0].type == MarkdownBlockType::Quote);
  CHECK(blocks[0].runs[0][0] == run(MarkdownInlineStyle::Text, "first second"));
  CHECK_EQ(blocks[0].raw, std::string("> first\n>second"));
}

TEST(an_ordered_list_keeps_its_first_number) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("3. three\n4. four\n- bullet");
  CHECK_EQ(blocks.size(), std::size_t(2));
  CHECK(blocks[0].ordered);
  CHECK_EQ(blocks[0].start, 3.0);
  CHECK_EQ(blocks[0].runs.size(), std::size_t(2));
  CHECK(!blocks[1].ordered);
  CHECK_EQ(blocks[1].start, 1.0);
}

TEST(a_list_marker_may_sit_behind_three_spaces_and_no_more) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("- item\n   - nested\n    - too deep");
  CHECK_EQ(blocks.size(), std::size_t(2));
  CHECK_EQ(blocks[0].runs.size(), std::size_t(2));
  CHECK(blocks[1].type == MarkdownBlockType::Paragraph);
}

TEST(a_table_holds_its_header_and_rows_without_the_alignment_row) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("| a | b |\n|:--|--:|\n| 1 | 2 |\n| 3 | 4 |");
  CHECK_EQ(blocks.size(), std::size_t(1));
  CHECK(blocks[0].type == MarkdownBlockType::Table);
  CHECK(blocks[0].rowCellCounts == (std::vector<std::size_t>{2, 2, 2}));
  CHECK_EQ(blocks[0].runs.size(), std::size_t(6));
  CHECK(blocks[0].runs[0][0] == run(MarkdownInlineStyle::Text, "a"));
  CHECK(blocks[0].runs[5][0] == run(MarkdownInlineStyle::Text, "4"));
}

TEST(a_table_row_still_streaming_in_has_fewer_cells) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("| a | b | c |\n|---|---|---|\n| 1 | 2");
  CHECK(blocks[0].rowCellCounts == (std::vector<std::size_t>{3, 2}));
  // A bare pipe is a table whose header is one empty cell.
  std::vector<MarkdownBlock> bare = parseMarkdown("|");
  CHECK(bare[0].rowCellCounts == (std::vector<std::size_t>{1}));
  CHECK(bare[0].runs[0].empty());
}

TEST(a_table_row_that_holds_a_line_terminator_ends_the_parse) {
  // The carriage return of a Windows line ending, before its line feed arrives.
  std::vector<MarkdownBlock> blocks = parseMarkdown("| a | b |\r");
  CHECK_EQ(blocks.size(), std::size_t(1));
  CHECK(blocks[0].type == MarkdownBlockType::Table);
  CHECK(blocks[0].rowCellCounts == (std::vector<std::size_t>{2}));
  CHECK_EQ(parseMarkdown("|a\xE2\x80\xA8" "b|\n|c|").size(), std::size_t(1));
}

TEST(markdown_splits_on_windows_line_endings) {
  std::vector<MarkdownBlock> blocks = parseMarkdown("# Title\r\n\r\ntext\r\nmore\r\n");
  CHECK_EQ(blocks.size(), std::size_t(2));
  CHECK(blocks[0].runs[0][0] == run(MarkdownInlineStyle::Text, "Title"));
  CHECK_EQ(blocks[1].raw, std::string("text\nmore"));
}

TEST(markdown_reads_the_four_inline_styles) {
  std::vector<MarkdownInline> expected = {
    run(MarkdownInlineStyle::Text, "a "),
    run(MarkdownInlineStyle::Bold, "bold"),
    run(MarkdownInlineStyle::Text, " "),
    run(MarkdownInlineStyle::Italic, "it"),
    run(MarkdownInlineStyle::Text, " "),
    run(MarkdownInlineStyle::Italic, "too"),
    run(MarkdownInlineStyle::Text, " "),
    run(MarkdownInlineStyle::Code, "code"),
    run(MarkdownInlineStyle::Text, " "),
    link("docs", "https://example.com"),
    run(MarkdownInlineStyle::Text, " now"),
  };
  CHECK(runsOf("a **bold** *it* _too_ `code` [docs](https://example.com) now") == expected);
}

TEST(an_inline_opener_without_a_closer_runs_to_the_end) {
  CHECK(runsOf("a **bold")[1] == run(MarkdownInlineStyle::Bold, "bold"));
  CHECK(runsOf("a `code")[1] == run(MarkdownInlineStyle::Code, "code"));
  CHECK(runsOf("an *italic")[1] == run(MarkdownInlineStyle::Italic, "italic"));
  // A link whose target is still streaming in has no href yet.
  MarkdownInline open = runsOf("see [docs](https://exa")[1];
  CHECK(open.style == MarkdownInlineStyle::Link);
  CHECK_EQ(open.text, std::string("docs"));
  CHECK(!open.hasHref);
  CHECK(open.href.empty());
}

TEST(arithmetic_and_snake_case_stay_literal) {
  std::vector<MarkdownInline> runs = runsOf("2 * 3 and snake_case_name");
  CHECK_EQ(runs.size(), std::size_t(1));
  CHECK(runs[0] == run(MarkdownInlineStyle::Text, "2 * 3 and snake_case_name"));
}

TEST(a_bracket_that_is_no_link_stays_text) {
  CHECK(runsOf("array[0] and [note] (later)") ==
    (std::vector<MarkdownInline>{run(MarkdownInlineStyle::Text, "array[0] and [note] (later)")}));
}

TEST(markdown_keeps_text_outside_ascii_whole) {
  std::vector<MarkdownBlock> blocks =
    parseMarkdown("#\xC2\xA0\xC3\x9C" "ber\n\ncaf\xC3\xA9 **\xE6\x97\xA5\xE6\x9C\xAC**");
  // A no-break space after the mark counts as white space.
  CHECK(blocks[0].type == MarkdownBlockType::Heading);
  CHECK(blocks[0].runs[0][0] == run(MarkdownInlineStyle::Text, "\xC3\x9C" "ber"));
  CHECK(blocks[1].runs[0][1] == run(MarkdownInlineStyle::Bold, "\xE6\x97\xA5\xE6\x9C\xAC"));
}

TEST(every_prefix_of_a_document_parses) {
  std::string source =
    "# Plan\n\n1. **Book** [flight](https://x.io)\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```js\nx\n```\n> quote";
  for (std::size_t end = 0; end <= source.size(); ++end) {
    std::vector<MarkdownBlock> blocks = parseMarkdown(std::string_view(source).substr(0, end));
    CHECK(blocks.size() <= 5);
  }
  CHECK_EQ(parseMarkdown(source).size(), std::size_t(5));
}

TEST(a_streamed_parse_matches_a_full_parse) {
  for (const std::string& source : streamedDocuments()) {
    for (std::size_t step : {1, 2, 3, 7}) {
      std::vector<std::size_t> cuts;
      for (std::size_t end = 0; end <= source.size(); end += step) {
        cuts.push_back(end);
      }
      cuts.push_back(source.size());
      checkStreamedParse(source, cuts);
    }
  }
}

TEST(blocks_before_the_last_line_keep_their_place) {
  MarkdownDocument document;
  CHECK_EQ(document.update("# Title\n\nfirst para\n\nsec"), std::size_t(0));
  CHECK_EQ(document.getBlocks().size(), std::size_t(3));
  // The heading and the first paragraph were decided on finished lines.
  CHECK_EQ(document.update("# Title\n\nfirst para\n\nsecond"), std::size_t(2));
  CHECK_EQ(document.getBlocks()[2].raw, std::string("second"));
  // The same text again parses nothing.
  CHECK_EQ(document.update("# Title\n\nfirst para\n\nsecond"), std::size_t(3));
}

TEST(a_block_that_ends_on_the_last_line_is_parsed_again) {
  MarkdownDocument document;
  document.update("# Title");
  CHECK_EQ(document.update("# Title\n"), std::size_t(0));
  // The parser looked at the unfinished second line to end the heading.
  CHECK_EQ(document.update("# Title\nnext"), std::size_t(0));
  CHECK_EQ(document.update("# Title\nnext\n\nlast"), std::size_t(0));
  CHECK_EQ(document.update("# Title\nnext\n\nlast\nmore"), std::size_t(2));
}

TEST(text_that_is_no_continuation_is_parsed_from_the_start) {
  MarkdownDocument document;
  document.update("# Title\n\nfirst para\n\nsecond");
  CHECK_EQ(document.update("# Other"), std::size_t(0));
  CHECK(document.getBlocks() == parseMarkdown("# Other"));
  // A shorter text is no continuation either.
  CHECK_EQ(document.update("# Oth"), std::size_t(0));
  CHECK(document.getBlocks() == parseMarkdown("# Oth"));
}

TEST(only_web_and_mail_links_are_safe_to_open) {
  CHECK(isSafeUrl("https://example.com"));
  CHECK(isSafeUrl("http://example.com"));
  CHECK(isSafeUrl("mailto:someone@example.com"));
  CHECK(isSafeUrl("  HTTPS://EXAMPLE.COM\n"));
  CHECK(!isSafeUrl("tel:+15551234"));
  CHECK(!isSafeUrl("javascript:alert(1)"));
  CHECK(!isSafeUrl("myapp://open"));
  CHECK(!isSafeUrl("see https://example.com"));
  CHECK(!isSafeUrl("https"));
  CHECK(!isSafeUrl(""));
}

/*
 * Documents built from every block and inline kind, cut at random bytes, a code point's middle
 * included, and fed to one MarkdownDocument cut by cut. Every step must give the blocks a parse
 * of the whole prefix gives and keep the blocks it says it kept.
 */
TEST(fuzz_markdown_prefixes_parse_like_the_whole) {
  FuzzKinds kinds = {
    {"heading", "paragraph", "quote", "bullet list", "ordered list", "closed fence", "open fence", "table", "rule",
      "blank line", "marker alone", "windows line ending", "plain", "bold", "italic", "code", "link", "open style",
      "text outside ascii", "whole document again", "other document"},
    {},
    {},
  };
  forEachFuzzSeed(__func__, MARKDOWN_FUZZ_SEEDS, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    std::mt19937 rng(seed);
    static const std::vector<std::string> words = {"a", "list", "of", "words", "x_y", "2", "*", "3", "-", "#", "|",
      ">", "1.", "caf\xC3\xA9", "\xF0\x9F\x98\x80", "\xC2\xA0", "```", "``", "`"};

    auto inlineText = [&] {
      std::string text;
      std::size_t count = pickSize(rng, 1, 5);
      for (std::size_t index = 0; index < count; ++index) {
        if (index > 0) {
          text += " ";
        }
        std::string word = words[pickSize(rng, 0, words.size() - 1)];
        if (word.size() > 1 && static_cast<unsigned char>(word[0]) >= 0x80) {
          episode.reach("text outside ascii");
        }
        int roll = pickPercent(rng);
        if (roll < 50) {
          episode.reach("plain");
          text += word;
        } else if (roll < 62) {
          episode.reach("bold");
          text += "**" + word + "**";
        } else if (roll < 74) {
          episode.reach("italic");
          text += (chance(rng, 50) ? "*" : "_") + word + (chance(rng, 50) ? "*" : "_");
        } else if (roll < 84) {
          episode.reach("code");
          text += "`" + word + "`";
        } else if (roll < 94) {
          episode.reach("link");
          text += "[" + word + "](https://x.io/" + word + ")";
        } else {
          episode.reach("open style");
          text += (chance(rng, 50) ? "**" : "[label](") + word;
        }
      }
      return text;
    };

    bool windows = chance(rng, 25);
    if (windows) {
      episode.reach("windows line ending");
    }
    std::string newline = windows ? "\r\n" : "\n";
    std::string source;
    std::size_t blockCount = pickSize(rng, 1, 8);
    for (std::size_t block = 0; block < blockCount; ++block) {
      int roll = pickPercent(rng);
      std::size_t lineCount = pickSize(rng, 1, 3);
      if (roll < 12) {
        episode.reach("heading");
        source += std::string(pickSize(rng, 1, 7), '#') + " " + inlineText() + newline;
      } else if (roll < 30) {
        episode.reach("paragraph");
        for (std::size_t line = 0; line < lineCount; ++line) {
          source += inlineText() + newline;
        }
      } else if (roll < 40) {
        episode.reach("quote");
        for (std::size_t line = 0; line < lineCount; ++line) {
          source += (chance(rng, 50) ? "> " : ">") + inlineText() + newline;
        }
      } else if (roll < 52) {
        episode.reach("bullet list");
        for (std::size_t line = 0; line < lineCount; ++line) {
          source += std::string(pickSize(rng, 0, 4), ' ') + (chance(rng, 50) ? "- " : "* ") + inlineText() + newline;
        }
      } else if (roll < 62) {
        episode.reach("ordered list");
        for (std::size_t line = 0; line < lineCount; ++line) {
          source += std::to_string(pickSize(rng, 0, 120)) + ". " + inlineText() + newline;
        }
      } else if (roll < 72) {
        bool closed = chance(rng, 60);
        episode.reach(closed ? "closed fence" : "open fence");
        source += "```" + std::string(chance(rng, 50) ? "ts" : "") + newline;
        for (std::size_t line = 0; line < lineCount; ++line) {
          source += (chance(rng, 30) ? "" : inlineText()) + newline;
        }
        source += closed ? "```" + newline : (chance(rng, 50) ? "``" : "");
      } else if (roll < 82) {
        episode.reach("table");
        std::size_t columns = pickSize(rng, 1, 4);
        for (std::size_t line = 0; line < lineCount + 1; ++line) {
          for (std::size_t column = 0; column < columns; ++column) {
            source += "|" + (line == 1 ? std::string(chance(rng, 50) ? "---" : ":-:") : " " + inlineText() + " ");
          }
          source += (chance(rng, 80) ? "|" : "") + newline;
        }
      } else if (roll < 88) {
        episode.reach("rule");
        source += std::string(pickSize(rng, 2, 5), chance(rng, 50) ? '-' : '*') + newline;
      } else if (roll < 94) {
        episode.reach("marker alone");
        static const std::vector<std::string> markers = {"#", "-", "*", "1.", ">", "|", "```", "``", "`"};
        source += markers[pickSize(rng, 0, markers.size() - 1)] + newline;
      } else {
        episode.reach("blank line");
        source += std::string(pickSize(rng, 0, 2), ' ') + newline;
      }
      if (chance(rng, 60)) {
        source += newline;
      }
    }

    std::vector<std::size_t> cuts;
    std::size_t cut = 0;
    while (cut < source.size()) {
      cut = std::min(source.size(), cut + pickSize(rng, 1, 12));
      cuts.push_back(cut);
      if (chance(rng, 5)) {
        episode.reach("whole document again");
        cuts.push_back(cut);
      }
    }
    checkStreamedParse(source, cuts);

    for (const MarkdownBlock& block : parseMarkdown(source)) {
      episode.observe(block.key);
      episode.observe(block.raw);
      episode.observe(block.runs.size());
    }

    // A document that held another text starts over and still matches a parse of the whole.
    if (chance(rng, 30)) {
      episode.reach("other document");
      MarkdownDocument document;
      document.update(source);
      std::string other = "intro" + newline + source;
      std::size_t kept = document.update(other);
      if (kept != 0 || document.getBlocks() != parseMarkdown(other)) {
        fail("a document given another text did not start over");
      }
    }
  });
}
