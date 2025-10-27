package com.shadowlist

import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.ViewGroupManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.ViewManagerDelegate
import com.facebook.react.uimanager.annotations.ReactProp
import com.facebook.react.viewmanagers.ShadowlistElementViewManagerInterface
import com.facebook.react.viewmanagers.ShadowlistElementViewManagerDelegate

@ReactModule(name = ShadowlistElementViewManager.NAME)
class ShadowlistElementViewManager : ViewGroupManager<ShadowlistElementView>(),
  ShadowlistElementViewManagerInterface<ShadowlistElementView> {
  private val mDelegate: ViewManagerDelegate<ShadowlistElementView>

  init {
    mDelegate = ShadowlistElementViewManagerDelegate(this)
  }

  override fun getDelegate(): ViewManagerDelegate<ShadowlistElementView>? {
    return mDelegate
  }

  override fun getName(): String {
    return NAME
  }

  public override fun createViewInstance(context: ThemedReactContext): ShadowlistElementView {
    return ShadowlistElementView(context)
  }

  @ReactProp(name = "index")
  override fun setIndex(view: ShadowlistElementView?, index: Int) {

  }

  companion object {
    const val NAME = "ShadowlistElementView"
  }
}
