package com.shadowlist.utils

import android.graphics.Rect
import com.shadowlist.kit.ShadowListKitTextLayout

class ShadowListUtilsBoardCardLayout internal constructor(
  widthPx: Int, heightPx: Int, val card: Rect, val accent: Rect, val title: ShadowListKitTextLayout, val titleFrame: Rect,
  val detail: ShadowListKitTextLayout?, val detailFrame: Rect, val due: ShadowListKitTextLayout?, val dueFrame: Rect,
  val dotFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)
