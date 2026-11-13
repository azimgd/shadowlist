package com.shadowlist.utils

import android.content.Context

/*
 * The model of what one cell shows. Its layout is a pure function of the row and a width, safe
 * to compute off the UI thread, and its view only copies the frames the layout holds.
 */
interface ShadowListUtilsRow {
  /*
   * The row's identity.
   */
  val key: String

  /*
   * Changes when the row's content changes under the same key, like a hash of the displayed
   * fields. The list then configures that cell again.
   */
  val contentVersion: Int get() = 0

  /*
   * Makes the cell that shows the row. The list registers it the first time a row with the
   * identifier arrives.
   */
  fun makeView(context: Context): ShadowListUtilsRowView

  /*
   * Names the view class. Rows with the same identifier share cells.
   */
  val reuseIdentifier: String

  /*
   * The row's frames for the size the list gives it across its scroll axis, in pixels: the
   * cell's width. In a horizontal list widthPx is the list's height, the layout's width is the
   * row's size along the list, and the layout's height is the height of the row's content.
   */
  fun layout(widthPx: Int): ShadowListUtilsRowLayout
}
