package com.shadowlist.utils

/*
 * A typed copy of the core's MarkdownInline. Style order mirrors MarkdownInlineStyle.
 */
internal class ShadowListUtilsMarkdownInline(style: Int, val text: String, val href: String?) {
  enum class Style { TEXT, BOLD, ITALIC, CODE, LINK }
  val style: Style = Style.entries[style]
}
