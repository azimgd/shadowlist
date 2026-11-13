package com.shadowlist.utils

import android.icu.text.BreakIterator
import java.text.Collator

/*
 * Contacts grouped by the first character of their name. The title is that character after
 * trimming, upper cased, or # when the name is empty or the character is not a letter. A
 * character is a whole grapheme cluster, like a letter with its accents. Sections sort by title
 * in the reader's locale with # last, and items keep their order inside a section. The section
 * key is the title.
 */
fun shadowListUtilsGroupIntoSections(items: List<ShadowListUtilsContactItem>): List<ShadowListUtilsContactSection> {
  val otherTitle = ShadowListUtilsCore.SECTION_OTHER_TITLE
  val groups = LinkedHashMap<String, ArrayList<ShadowListUtilsContactItem>>()
  for (item in items) groups.getOrPut(sectionTitle(item.name, otherTitle)) { ArrayList() }.add(item)
  val collator = Collator.getInstance()
  val titles = groups.keys.sortedWith { first, second ->
    when {
      first == second -> 0
      first == otherTitle -> 1
      second == otherTitle -> -1
      else -> collator.compare(first, second)
    }
  }
  return titles.map { ShadowListUtilsContactSection(it, it, groups.getValue(it)) }
}

/*
 * The core answers with the first code point: an ASCII letter in upper case, # for a name that
 * starts with no letter, and a code point outside ASCII as written. The title is the whole
 * grapheme cluster that code point starts, like a letter with its accents, in upper case when
 * it is a letter.
 */
private fun sectionTitle(name: String, otherTitle: String): String {
  val initial = ShadowListUtilsCore.sectionInitial(name)
  if (initial.isEmpty() || initial == otherTitle) return otherTitle
  val codePoint = initial.codePointAt(0)
  val fallback = if (codePoint < ASCII_END) initial else otherTitle
  var start = 0
  while (start < name.length && Character.toUpperCase(name.codePointAt(start)) != Character.toUpperCase(codePoint)) {
    start += Character.charCount(name.codePointAt(start))
  }
  if (start >= name.length || !Character.isLetter(name.codePointAt(start))) return fallback
  val clusters = BreakIterator.getCharacterInstance()
  clusters.setText(name)
  val end = clusters.following(start)
  return name.substring(start, if (end == BreakIterator.DONE) name.length else end).uppercase()
}

private const val ASCII_END = 0x80
