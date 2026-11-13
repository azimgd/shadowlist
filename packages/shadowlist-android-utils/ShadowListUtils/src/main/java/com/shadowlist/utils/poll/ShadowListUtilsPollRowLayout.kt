package com.shadowlist.utils

import android.graphics.Rect
import com.shadowlist.kit.ShadowListKitTextLayout

/*
 * The frames and typeset text of a poll option.
 */
class ShadowListUtilsPollRowLayout internal constructor(
  widthPx: Int, heightPx: Int,
  val selectionFrame: Rect,
  val initial: ShadowListKitTextLayout, val initialFrame: Rect,
  val label: ShadowListKitTextLayout, val labelFrame: Rect,
  val percent: ShadowListKitTextLayout, val percentFrame: Rect,
  val barFrame: Rect, val barFillFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)
