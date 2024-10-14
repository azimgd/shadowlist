package com.shadowlist

import android.graphics.Color
import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.ViewManagerDelegate
import com.facebook.react.uimanager.annotations.ReactProp
import com.facebook.react.viewmanagers.ShadowlistViewManagerInterface
import com.facebook.react.viewmanagers.ShadowlistViewManagerDelegate

@ReactModule(name = ShadowlistViewManager.NAME)
class ShadowlistViewManager : SimpleViewManager<ShadowlistView>(),
  ShadowlistViewManagerInterface<ShadowlistView> {
  private val mDelegate: ViewManagerDelegate<ShadowlistView>

  init {
    mDelegate = ShadowlistViewManagerDelegate(this)
  }

  override fun getDelegate(): ViewManagerDelegate<ShadowlistView>? {
    return mDelegate
  }

  override fun getName(): String {
    return NAME
  }

  public override fun createViewInstance(context: ThemedReactContext): ShadowlistView {
    return ShadowlistView(context)
  }

  @ReactProp(name = "color")
  override fun setColor(view: ShadowlistView?, color: String?) {
    view?.setBackgroundColor(Color.parseColor(color))
  }

  companion object {
    const val NAME = "ShadowlistView"
  }
}
