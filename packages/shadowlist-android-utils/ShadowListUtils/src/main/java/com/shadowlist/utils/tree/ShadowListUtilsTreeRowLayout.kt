package com.shadowlist.utils

import android.graphics.Rect
import com.shadowlist.kit.ShadowListKitTextLayout

class ShadowListUtilsTreeRowLayout internal constructor(
  widthPx: Int,
  heightPx: Int,
  val name: ShadowListKitTextLayout,
  val nameFrame: Rect,
  val chevronFrame: Rect,
  val iconFrame: Rect,
  val count: ShadowListKitTextLayout?,
  val countFrame: Rect,
) : ShadowListUtilsRowLayout(widthPx, heightPx)
