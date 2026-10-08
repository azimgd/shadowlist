#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * What a row the core places shows: an item, or a section's header or footer.
 */
enum class RowKind {
  Item,
  Header,
  Footer,
};

/*
 * One section as the data source gives it.
 */
struct SectionSpec {
  std::size_t itemCount = 0;
  bool hasHeader = false;
  bool hasFooter = false;
};

/*
 * A row's place in the sections. Item is the item index across all sections, or
 * UNDEFINED_INDEX for a header or footer row.
 */
struct RowPlace {
  RowKind kind = RowKind::Item;
  std::size_t section = 0;
  std::size_t item = UNDEFINED_INDEX;
};

/*
 * Sections over the flat rows the core places. A section is its header row, its items and its
 * footer row, in that order, each part optional. Items are numbered across all sections, which
 * keeps the item indices of a list with sections the same as of one without. A list without
 * sections maps every row to the item with the same index.
 */
class ListSections final {
public:
  /*
   * Rows of a header and footer get keys from the section key with these prefixes. Item keys
   * that start with them would collide.
   */
  static constexpr const char* HEADER_KEY_PREFIX = "\x1fh:";
  static constexpr const char* FOOTER_KEY_PREFIX = "\x1f" "f:";

  /*
   * Back to a list without sections of count items.
   */
  void setPlain(std::size_t count);
  void setSections(std::vector<SectionSpec> sections);

  bool isSectioned() const { return sectioned_; }
  std::size_t getSectionCount() const { return sections_.size(); }
  std::size_t getItemCount() const { return itemCount_; }
  std::size_t getRowCount() const { return rowCount_; }

  RowPlace placeOfRow(std::size_t row) const;

  /*
   * The item a row shows, or UNDEFINED_INDEX for a header, a footer or a row past the end.
   */
  std::size_t itemForRow(std::size_t row) const;

  /*
   * The row of an item, or UNDEFINED_INDEX past the end.
   */
  std::size_t rowForItem(std::size_t item) const;

  /*
   * The section an item is in, or UNDEFINED_INDEX past the end.
   */
  std::size_t sectionForItem(std::size_t item) const;

  /*
   * The item index the section's items start at. An empty section starts where the next one does.
   */
  std::size_t firstItemInSection(std::size_t section) const;
  std::size_t itemCountInSection(std::size_t section) const;

  /*
   * The section's header or footer row, or UNDEFINED_INDEX when it has none.
   */
  std::size_t headerRow(std::size_t section) const;
  std::size_t footerRow(std::size_t section) const;

  /*
   * The first row of a section, its header when it has one, or UNDEFINED_INDEX for a section
   * without rows.
   */
  std::size_t firstRowInSection(std::size_t section) const;

  /*
   * Every header row, low to high.
   */
  std::vector<std::size_t> headerRows() const;

  /*
   * Whether the row is an item followed by another item of the same section, where a
   * separator goes.
   */
  bool isItemBeforeItem(std::size_t row) const;

  /*
   * The item a row dragged from fromRow and dropped at toRow becomes, where toRow is its row in
   * the order after the move. A row stays in its own section: a drop outside goes to the nearest
   * end of it. UNDEFINED_INDEX when fromRow is no item.
   */
  std::size_t itemForDrop(std::size_t fromRow, std::size_t toRow) const;

  /*
   * The rows' keys: each section's header key, its items' keys and its footer key.
   * itemKeys has one key per item, sectionKeys one per section.
   */
  std::vector<std::string> rowKeys(
    const std::vector<std::string>& itemKeys,
    const std::vector<std::string>& sectionKeys) const;

  /*
   * The items' keys out of the rows' keys, skipping headers and footers.
   */
  std::vector<std::string> itemKeys(const std::vector<std::string>& rowKeys) const;

private:
  /*
   * The last section starting at or before value in starts. Sections without rows or items
   * share their start with the next one, and the search lands on the last of them.
   */
  std::size_t sectionAt(const std::vector<std::size_t>& starts, std::size_t value) const;

  bool sectioned_ = false;
  std::vector<SectionSpec> sections_;
  std::vector<std::size_t> rowStarts_;
  std::vector<std::size_t> itemStarts_;
  std::size_t itemCount_ = 0;
  std::size_t rowCount_ = 0;
};

}
