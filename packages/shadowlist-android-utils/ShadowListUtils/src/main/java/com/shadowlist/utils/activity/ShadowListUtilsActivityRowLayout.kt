package com.shadowlist.utils

import android.graphics.Rect
import com.shadowlist.kit.ShadowListKitTextLayout

/*
 * The frames and typeset text of an activity row.
 */
class ShadowListUtilsActivityRowLayout internal constructor(
  widthPx: Int, heightPx: Int,
  val avatarFrame: Rect, val unreadDotFrame: Rect,
  val title: ShadowListKitTextLayout, val titleFrame: Rect,
  val text: ShadowListKitTextLayout?, val textFrame: Rect,
  val time: ShadowListKitTextLayout, val timeFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)
