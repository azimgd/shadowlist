package com.shadowlist.kit.example

import com.shadowlist.utils.ShadowListUtilsImageLoader
import com.shadowlist.utils.ShadowListUtilsImageView

/*
 * Leaves every image view on its placeholder color.
 */
object NoImageLoader : ShadowListUtilsImageLoader() {
  override fun load(url: String, into: ShadowListUtilsImageView) {}
  override fun cancel(view: ShadowListUtilsImageView) {}
}
