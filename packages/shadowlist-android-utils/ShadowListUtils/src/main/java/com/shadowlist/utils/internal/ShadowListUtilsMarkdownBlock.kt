package com.shadowlist.utils

/*
 * A typed copy of the core's MarkdownBlock. Type order mirrors MarkdownBlockType.
 * Runs are grouped by list item or table cell; rowCellCounts preserves streaming short rows.
 */
internal class ShadowListUtilsMarkdownBlock(
  type: Int,
  val key: String,
  val raw: String,
  val runs: Array<Array<ShadowListUtilsMarkdownInline>>,
  val rowCellCounts: IntArray,
  val level: Int,
  val ordered: Boolean,
  val start: Double,
  val language: String,
  val code: String,
  val closed: Boolean,
) {
  enum class Type { HEADING, PARAGRAPH, QUOTE, LIST, CODE, TABLE, RULE }
  val type: Type = Type.entries[type]
}
