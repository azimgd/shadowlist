package com.shadowlist

import com.facebook.react.bridge.ReadableArray
import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.ViewGroupManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.ViewManagerDelegate
import com.facebook.react.uimanager.annotations.ReactProp
import com.facebook.react.viewmanagers.ShadowlistViewManagerInterface
import com.facebook.react.viewmanagers.ShadowlistViewManagerDelegate

@ReactModule(name = ShadowlistViewManager.NAME)
class ShadowlistViewManager : ViewGroupManager<ShadowlistView>(),
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

  @ReactProp(name = "elementsAllKeys")
  override fun setElementsAllKeys(
    view: ShadowlistView?,
    value: ReadableArray?
  ) {

  }

  @ReactProp(name = "elementsHeadKey")
  override fun setElementsHeadKey(view: ShadowlistView?, value: String?) {

  }

  @ReactProp(name = "elementsTailKey")
  override fun setElementsTailKey(view: ShadowlistView?, value: String?) {

  }

  @ReactProp(name = "inverted")
  override fun setInverted(view: ShadowlistView?, value: Boolean) {

  }

  @ReactProp(name = "horizontal")
  override fun setHorizontal(view: ShadowlistView?, value: Boolean) {

  }

  override fun prependElements(view: ShadowlistView?, size: Int) {

  }

  override fun appendElements(view: ShadowlistView?, size: Int) {

  }

  companion object {
    const val NAME = "ShadowlistView"
  }
}
