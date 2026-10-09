package com.shadowlist.kit

import android.graphics.Canvas

/*
 * Draws with the rows, like RecyclerView's ItemDecoration. onDraw runs before the cells draw,
 * onDrawOver after, both in the list's scrolled coordinates. visibleCells gives the cells to
 * draw for. Decorations take no space.
 */
interface ShadowListKitItemDecoration {
  fun onDraw(canvas: Canvas, listView: ShadowListKitListView) {}
  fun onDrawOver(canvas: Canvas, listView: ShadowListKitListView) {}
}
