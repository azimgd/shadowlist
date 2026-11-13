/*
 * Contact section tests: the section a contact's name falls in, and how contacts group into
 * sections.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/utils/ContactSection.hpp>

#include <cstddef>
#include <string>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

std::vector<std::string> titlesOf(const std::vector<ContactSection>& sections) {
  std::vector<std::string> titles;
  for (const ContactSection& section : sections) {
    titles.push_back(section.title);
  }
  return titles;
}

}

TEST(a_contact_section_is_titled_with_the_first_letter_in_upper_case) {
  CHECK_EQ(sectionInitial("Ada"), std::string("A"));
  CHECK_EQ(sectionInitial("bob"), std::string("B"));
  CHECK_EQ(sectionInitial("z"), std::string("Z"));
}

TEST(a_contact_section_title_skips_the_white_space_before_the_name) {
  CHECK_EQ(sectionInitial("  Bea"), std::string("B"));
  // A no-break space, then a tab.
  CHECK_EQ(sectionInitial("\xC2\xA0\tcy "), std::string("C"));
}

TEST(a_name_that_starts_with_no_letter_falls_in_the_other_section) {
  CHECK_EQ(sectionInitial(""), std::string(SECTION_OTHER_TITLE));
  CHECK_EQ(sectionInitial("   "), std::string(SECTION_OTHER_TITLE));
  CHECK_EQ(sectionInitial("42"), std::string(SECTION_OTHER_TITLE));
  CHECK_EQ(sectionInitial("_x"), std::string(SECTION_OTHER_TITLE));
  CHECK_EQ(sectionInitial("#1"), std::string(SECTION_OTHER_TITLE));
  CHECK_EQ(sectionInitial("@home"), std::string(SECTION_OTHER_TITLE));
  CHECK_EQ(sectionInitial("[x]"), std::string(SECTION_OTHER_TITLE));
  CHECK_EQ(sectionInitial("{x}"), std::string(SECTION_OTHER_TITLE));
}

TEST(a_contact_section_title_outside_ascii_is_left_to_the_host) {
  // U+00E9, and U+1F600 as one code point.
  CHECK_EQ(sectionInitial("\xC3\xA9mile"), std::string("\xC3\xA9"));
  CHECK_EQ(sectionInitial(" \xF0\x9F\x98\x80 Ada"), std::string("\xF0\x9F\x98\x80"));
}

TEST(contacts_group_by_title_and_keep_their_order_inside_a_section) {
  std::vector<ContactSection> sections = groupContactsIntoSections({"B", "A", "B", "A", "C"});
  CHECK(titlesOf(sections) == (std::vector<std::string>{"A", "B", "C"}));
  CHECK(sections[0].items == (std::vector<std::size_t>{1, 3}));
  CHECK(sections[1].items == (std::vector<std::size_t>{0, 2}));
  CHECK(sections[2].items == (std::vector<std::size_t>{4}));
}

TEST(the_other_contact_section_goes_last) {
  std::vector<ContactSection> sections = groupContactsIntoSections({"#", "Z", "#", "A"});
  CHECK(titlesOf(sections) == (std::vector<std::string>{"A", "Z", "#"}));
  CHECK(sections[2].items == (std::vector<std::size_t>{0, 2}));
  CHECK(titlesOf(groupContactsIntoSections({"#"})) == (std::vector<std::string>{"#"}));
}

TEST(no_contacts_group_into_no_sections) {
  CHECK(groupContactsIntoSections({}).empty());
}

TEST(contact_sections_sort_with_the_order_the_host_gives) {
  // A host whose locale puts the titles in reverse. The other section still goes last.
  std::vector<ContactSection> sections = groupContactsIntoSections(
    {"A", "#", "C", "B"},
    [](const std::string& first, const std::string& second) { return first > second; });
  CHECK(titlesOf(sections) == (std::vector<std::string>{"C", "B", "A", "#"}));
  CHECK(sections[3].items == (std::vector<std::size_t>{1}));
}
