#include <shadowlist-core/host/ListSections.hpp>

#include <algorithm>
#include <utility>

namespace azimgd::shadowlist {

void ListSections::setPlain(std::size_t count) {
  sectioned_ = false;
  sections_.clear();
  rowStarts_.clear();
  itemStarts_.clear();
  itemCount_ = count;
  rowCount_ = count;
}

void ListSections::setSections(std::vector<SectionSpec> sections) {
  sectioned_ = true;
  sections_ = std::move(sections);
  rowStarts_.resize(sections_.size());
  itemStarts_.resize(sections_.size());
  std::size_t row = 0;
  std::size_t item = 0;
  for (std::size_t section = 0; section < sections_.size(); ++section) {
    const SectionSpec& spec = sections_[section];
    rowStarts_[section] = row;
    itemStarts_[section] = item;
    row += spec.itemCount + (spec.hasHeader ? 1 : 0) + (spec.hasFooter ? 1 : 0);
    item += spec.itemCount;
  }
  itemCount_ = item;
  rowCount_ = row;
}

std::size_t ListSections::sectionAt(const std::vector<std::size_t>& starts, std::size_t value) const {
  auto after = std::upper_bound(starts.begin(), starts.end(), value);
  if (after == starts.begin()) {
    return UNDEFINED_INDEX;
  }
  return static_cast<std::size_t>(after - starts.begin()) - 1;
}

RowPlace ListSections::placeOfRow(std::size_t row) const {
  if (!sectioned_) {
    return {RowKind::Item, 0, row < rowCount_ ? row : UNDEFINED_INDEX};
  }
  if (row >= rowCount_) {
    return {RowKind::Item, UNDEFINED_INDEX, UNDEFINED_INDEX};
  }
  std::size_t section = sectionAt(rowStarts_, row);
  const SectionSpec& spec = sections_[section];
  std::size_t local = row - rowStarts_[section];
  if (spec.hasHeader) {
    if (local == 0) {
      return {RowKind::Header, section, UNDEFINED_INDEX};
    }
    --local;
  }
  if (local < spec.itemCount) {
    return {RowKind::Item, section, itemStarts_[section] + local};
  }
  return {RowKind::Footer, section, UNDEFINED_INDEX};
}

std::size_t ListSections::itemForRow(std::size_t row) const {
  return placeOfRow(row).item;
}

std::size_t ListSections::rowForItem(std::size_t item) const {
  if (item >= itemCount_) {
    return UNDEFINED_INDEX;
  }
  if (!sectioned_) {
    return item;
  }
  std::size_t section = sectionAt(itemStarts_, item);
  return rowStarts_[section] + (sections_[section].hasHeader ? 1 : 0) + (item - itemStarts_[section]);
}

std::size_t ListSections::sectionForItem(std::size_t item) const {
  if (item >= itemCount_) {
    return UNDEFINED_INDEX;
  }
  return sectioned_ ? sectionAt(itemStarts_, item) : 0;
}

std::size_t ListSections::firstItemInSection(std::size_t section) const {
  if (!sectioned_) {
    return section == 0 ? 0 : UNDEFINED_INDEX;
  }
  return section < sections_.size() ? itemStarts_[section] : UNDEFINED_INDEX;
}

std::size_t ListSections::itemCountInSection(std::size_t section) const {
  if (!sectioned_) {
    return section == 0 ? itemCount_ : 0;
  }
  return section < sections_.size() ? sections_[section].itemCount : 0;
}

std::size_t ListSections::headerRow(std::size_t section) const {
  if (!sectioned_ || section >= sections_.size() || !sections_[section].hasHeader) {
    return UNDEFINED_INDEX;
  }
  return rowStarts_[section];
}

std::size_t ListSections::footerRow(std::size_t section) const {
  if (!sectioned_ || section >= sections_.size() || !sections_[section].hasFooter) {
    return UNDEFINED_INDEX;
  }
  const SectionSpec& spec = sections_[section];
  return rowStarts_[section] + (spec.hasHeader ? 1 : 0) + spec.itemCount;
}

std::size_t ListSections::firstRowInSection(std::size_t section) const {
  if (!sectioned_) {
    return section == 0 && rowCount_ > 0 ? 0 : UNDEFINED_INDEX;
  }
  if (section >= sections_.size()) {
    return UNDEFINED_INDEX;
  }
  const SectionSpec& spec = sections_[section];
  bool hasRows = spec.hasHeader || spec.hasFooter || spec.itemCount > 0;
  return hasRows ? rowStarts_[section] : UNDEFINED_INDEX;
}

std::vector<std::size_t> ListSections::headerRows() const {
  std::vector<std::size_t> rows;
  for (std::size_t section = 0; section < sections_.size(); ++section) {
    if (sections_[section].hasHeader) {
      rows.push_back(rowStarts_[section]);
    }
  }
  return rows;
}

bool ListSections::isItemBeforeItem(std::size_t row) const {
  if (row + 1 >= rowCount_) {
    return false;
  }
  if (!sectioned_) {
    return true;
  }
  RowPlace place = placeOfRow(row);
  if (place.kind != RowKind::Item) {
    return false;
  }
  RowPlace next = placeOfRow(row + 1);
  return next.kind == RowKind::Item && next.section == place.section;
}

std::size_t ListSections::itemForDrop(std::size_t fromRow, std::size_t toRow) const {
  std::size_t fromItem = itemForRow(fromRow);
  if (fromItem == UNDEFINED_INDEX || !sectioned_) {
    return fromItem == UNDEFINED_INDEX ? UNDEFINED_INDEX : std::min(toRow, itemCount_ - 1);
  }
  // Items before the drop place in the order after the move, the dragged row left out.
  std::size_t itemsBefore = 0;
  std::size_t seen = 0;
  for (std::size_t row = 0; row < rowCount_ && seen < toRow; ++row) {
    if (row == fromRow) {
      continue;
    }
    ++seen;
    if (itemForRow(row) != UNDEFINED_INDEX) {
      ++itemsBefore;
    }
  }
  std::size_t section = sectionForItem(fromItem);
  std::size_t first = itemStarts_[section];
  std::size_t last = first + sections_[section].itemCount - 1;
  return std::min(std::max(itemsBefore, first), last);
}

std::vector<std::size_t> ListSections::itemsOfRows(const std::vector<std::size_t>& rows) const {
  std::vector<std::size_t> items;
  items.reserve(rows.size());
  for (std::size_t row : rows) {
    std::size_t item = itemForRow(row);
    if (item != UNDEFINED_INDEX) {
      items.push_back(item);
    }
  }
  return items;
}

std::optional<MountedRange> ListSections::itemRangeOfRows(std::size_t low, std::size_t high) const {
  MountedRange range;
  for (std::size_t row = low; row <= high && row < rowCount_; ++row) {
    std::size_t item = itemForRow(row);
    if (item == UNDEFINED_INDEX) {
      continue;
    }
    range.low = range.low == UNDEFINED_INDEX ? item : range.low;
    range.high = item;
  }
  if (range.low == UNDEFINED_INDEX) {
    return std::nullopt;
  }
  return range;
}

std::vector<std::size_t> ListSections::stickyRows(
  const std::vector<std::size_t>& stickyItems,
  bool sectionHeaders) const {
  std::vector<std::size_t> rows;
  for (std::size_t item : stickyItems) {
    std::size_t row = rowForItem(item);
    if (row != UNDEFINED_INDEX) {
      rows.push_back(row);
    }
  }
  if (sectionHeaders) {
    std::vector<std::size_t> headers = headerRows();
    rows.insert(rows.end(), headers.begin(), headers.end());
  }
  std::sort(rows.begin(), rows.end());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
  return rows;
}

std::string ListSections::sectionKey(
  std::size_t section,
  const std::optional<std::string>& given,
  const std::string& firstItemKey) const {
  if (given) {
    return *given;
  }
  if (section < sections_.size() && sections_[section].itemCount > 0) {
    return firstItemKey;
  }
  return "#" + std::to_string(section);
}

std::vector<std::string> ListSections::edgeRowKeys(
  const std::vector<std::optional<std::string>>& sectionKeys,
  const std::vector<std::string>& firstItemKeys) const {
  std::vector<std::string> keys;
  static const std::string NO_KEY;
  for (std::size_t section = 0; section < sections_.size(); ++section) {
    const SectionSpec& spec = sections_[section];
    if (!spec.hasHeader && !spec.hasFooter) {
      continue;
    }
    const std::optional<std::string>& given = section < sectionKeys.size() ? sectionKeys[section] : std::nullopt;
    const std::string& first = section < firstItemKeys.size() ? firstItemKeys[section] : NO_KEY;
    std::string key = sectionKey(section, given, first);
    if (spec.hasHeader) {
      keys.push_back(HEADER_KEY_PREFIX + key);
    }
    if (spec.hasFooter) {
      keys.push_back(FOOTER_KEY_PREFIX + key);
    }
  }
  return keys;
}

std::vector<std::string> ListSections::rowKeys(
  const std::vector<std::string>& itemKeys,
  const std::vector<std::optional<std::string>>& sectionKeys) const {
  if (!sectioned_) {
    return itemKeys;
  }
  static const std::string NO_KEY;
  std::vector<std::string> rows;
  rows.reserve(rowCount_);
  for (std::size_t section = 0; section < sections_.size(); ++section) {
    const SectionSpec& spec = sections_[section];
    std::size_t first = itemStarts_[section];
    const std::optional<std::string>& given = section < sectionKeys.size() ? sectionKeys[section] : std::nullopt;
    std::string key = sectionKey(section, given, first < itemKeys.size() ? itemKeys[first] : NO_KEY);
    if (spec.hasHeader) {
      rows.push_back(HEADER_KEY_PREFIX + key);
    }
    for (std::size_t local = 0; local < spec.itemCount; ++local) {
      std::size_t item = first + local;
      rows.push_back(item < itemKeys.size() ? itemKeys[item] : std::string());
    }
    if (spec.hasFooter) {
      rows.push_back(FOOTER_KEY_PREFIX + key);
    }
  }
  return rows;
}

std::vector<std::string> ListSections::itemKeys(const std::vector<std::string>& rowKeys) const {
  if (!sectioned_) {
    return rowKeys;
  }
  std::vector<std::string> items;
  items.reserve(itemCount_);
  for (std::size_t section = 0; section < sections_.size(); ++section) {
    const SectionSpec& spec = sections_[section];
    std::size_t first = rowStarts_[section] + (spec.hasHeader ? 1 : 0);
    for (std::size_t local = 0; local < spec.itemCount && first + local < rowKeys.size(); ++local) {
      items.push_back(rowKeys[first + local]);
    }
  }
  return items;
}

}
