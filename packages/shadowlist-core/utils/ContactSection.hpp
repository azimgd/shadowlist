#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace azimgd::shadowlist {

/*
 * The title of the section for names that start with no letter. It sorts after every other
 * section.
 */
constexpr std::string_view SECTION_OTHER_TITLE = "#";

/*
 * The title of the section a contact's name falls in: the first code point of the name after
 * trimming, as UTF-8. An ASCII letter comes back in upper case. A blank name, and a name that
 * starts with an ASCII character that is no letter, gives SECTION_OTHER_TITLE. A first
 * code point outside ASCII comes back as written. The host turns it to upper case when its own
 * string type, which knows every script, calls it a letter, and takes
 * SECTION_OTHER_TITLE for anything else.
 */
std::string sectionInitial(std::string_view name);

/*
 * One section of grouped contacts. The title is the section's key too. items are the indices
 * of its contacts, in the order the contacts came.
 */
struct ContactSection {
  std::string title;
  std::vector<std::size_t> items;
};

/*
 * Contacts grouped into sections. titles holds each contact's section title in the contacts'
 * order. Sections sort with isBefore(first, second), which says whether the title first goes
 * before the title second in the reader's locale. SECTION_OTHER_TITLE goes last and is
 * never handed to isBefore. Contacts keep their order inside a section.
 */
template <typename IsBefore>
std::vector<ContactSection> groupContactsIntoSections(
  const std::vector<std::string>& titles,
  const IsBefore& isBefore) {
  std::vector<ContactSection> sections;
  std::unordered_map<std::string_view, std::size_t> sectionOfTitle;
  for (std::size_t index = 0; index < titles.size(); ++index) {
    auto found = sectionOfTitle.find(titles[index]);
    if (found == sectionOfTitle.end()) {
      found = sectionOfTitle.emplace(titles[index], sections.size()).first;
      sections.push_back({titles[index], {}});
    }
    sections[found->second].items.push_back(index);
  }
  std::stable_sort(sections.begin(), sections.end(), [&](const ContactSection& first, const ContactSection& second) {
    if (first.title == SECTION_OTHER_TITLE || second.title == SECTION_OTHER_TITLE) {
      return first.title != SECTION_OTHER_TITLE;
    }
    return static_cast<bool>(isBefore(first.title, second.title));
  });
  return sections;
}

/*
 * The same with the titles in code point order, which is every locale's order for the upper
 * case ASCII letters.
 */
std::vector<ContactSection> groupContactsIntoSections(const std::vector<std::string>& titles);

}
