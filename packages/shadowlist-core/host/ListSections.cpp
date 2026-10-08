#include <shadowlist-core/host/ListSections.hpp>

#include <algorithm>

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

std::vector<std::string> ListSections::rowKeys(
  const std::vector<std::string>& itemKeys,
  const std::vector<std::string>& sectionKeys) const {
  if (!sectioned_) {
    return itemKeys;
  }
  std::vector<std::string> rows;
  rows.reserve(rowCount_);
  for (std::size_t section = 0; section < sections_.size(); ++section) {
    const SectionSpec& spec = sections_[section];
    const std::string sectionKey = section < sectionKeys.size() ? sectionKeys[section] : std::to_string(section);
    if (spec.hasHeader) {
      rows.push_back(HEADER_KEY_PREFIX + sectionKey);
    }
    for (std::size_t local = 0; local < spec.itemCount; ++local) {
      std::size_t item = itemStarts_[section] + local;
      rows.push_back(item < itemKeys.size() ? itemKeys[item] : std::string());
    }
    if (spec.hasFooter) {
      rows.push_back(FOOTER_KEY_PREFIX + sectionKey);
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
