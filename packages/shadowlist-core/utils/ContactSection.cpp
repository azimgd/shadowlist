#include <shadowlist-core/utils/ContactSection.hpp>

#include <shadowlist-core/utils/Text.hpp>

namespace azimgd::shadowlist {

namespace {

constexpr char32_t ASCII_END = 0x80;

}

std::string sectionInitial(std::string_view name) {
  std::string_view trimmed = trim(name);
  if (trimmed.empty()) {
    return std::string(SECTION_OTHER_TITLE);
  }
  std::size_t offset = 0;
  char32_t first = decodeUtf8(trimmed, offset);
  std::string title;
  if (first >= ASCII_END) {
    appendUtf8(title, first);
    return title;
  }
  if (first >= U'a' && first <= U'z') {
    first = first - U'a' + U'A';
  }
  if (first < U'A' || first > U'Z') {
    return std::string(SECTION_OTHER_TITLE);
  }
  appendUtf8(title, first);
  return title;
}

std::vector<ContactSection> groupContactsIntoSections(const std::vector<std::string>& titles) {
  return groupContactsIntoSections(titles, [](const std::string& first, const std::string& second) {
    return first < second;
  });
}

}
