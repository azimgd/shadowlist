package com.shadowlist;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ReactStylesDiffMap;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.ShadowlistViewManagerInterface;
import com.facebook.react.viewmanagers.ShadowlistViewManagerDelegate;

@ReactModule(name = ShadowlistViewManager.NAME)
public class ShadowlistViewManager extends ViewGroupManager<ShadowlistView>
  implements ShadowlistViewManagerInterface<ShadowlistView> {

  public static final String NAME = "ShadowlistView";

  private final ViewManagerDelegate<ShadowlistView> mDelegate;

  public ShadowlistViewManager() {
    mDelegate = new ShadowlistViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowlistView> getDelegate() {
    return mDelegate;
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  protected ShadowlistView createViewInstance(@NonNull ThemedReactContext context) {
    return new ShadowlistView(context);
  }

  /*
   * Element/template children are hosted in the content container inside the inner
   * scroll view, not directly on the host, so route Fabric's child mounting there.
   */
  @Override
  public void addView(ShadowlistView parent, View child, int index) {
    parent.addContentView(child, index);
  }

  @Override
  public int getChildCount(ShadowlistView parent) {
    return parent.getContentChildCount();
  }

  @Override
  public View getChildAt(ShadowlistView parent, int index) {
    return parent.getContentChildAt(index);
  }

  @Override
  public void removeViewAt(ShadowlistView parent, int index) {
    parent.removeContentViewAt(index);
  }

  @Override
  @ReactProp(name = "elementsAllKeys")
  public void setElementsAllKeys(ShadowlistView view, @Nullable ReadableArray elementsAllKeys) {
    // Consumed by the C++ core via props; no Android view state needed.
  }

  @Override
  @ReactProp(name = "inverted")
  public void setInverted(ShadowlistView view, boolean inverted) {
    // Consumed by the C++ core via props; no Android view state needed.
  }

  @Override
  @ReactProp(name = "horizontal")
  public void setHorizontal(ShadowlistView view, boolean horizontal) {
    view.setHorizontal(horizontal);
  }

  @Override
  @ReactProp(name = "stickyHeader")
  public void setStickyHeader(ShadowlistView view, boolean stickyHeader) {
    view.setStickyHeader(stickyHeader);
  }

  @Override
  @ReactProp(name = "stickyFooter")
  public void setStickyFooter(ShadowlistView view, boolean stickyFooter) {
    view.setStickyFooter(stickyFooter);
  }

  @Override
  public void setStartReachedEnabled(ShadowlistView view, boolean enabled) {
    view.setStartReachedEnabled(enabled);
  }

  @Override
  public void setEndReachedEnabled(ShadowlistView view, boolean enabled) {
    view.setEndReachedEnabled(enabled);
  }

  @Override
  @ReactProp(name = "columns")
  public void setColumns(ShadowlistView view, int columns) {
    // Consumed by the C++ core via props; no Android view state needed.
  }

  @Override
  @ReactProp(name = "containerOffsetIndex")
  public void setContainerOffsetIndex(ShadowlistView view, int containerOffsetIndex) {
    // Consumed by the C++ core via props; no Android view state needed.
  }

  @Override
  @ReactProp(name = "startReachedThreshold")
  public void setStartReachedThreshold(ShadowlistView view, double startReachedThreshold) {
    // Consumed by the C++ core via props; no Android view state needed.
  }

  @Override
  @ReactProp(name = "endReachedThreshold")
  public void setEndReachedThreshold(ShadowlistView view, double endReachedThreshold) {
    // Consumed by the C++ core via props; no Android view state needed.
  }

  @Override
  @ReactProp(name = "viewablePercentThreshold")
  public void setViewablePercentThreshold(ShadowlistView view, double viewablePercentThreshold) {
    // Consumed by the C++ core via props; no Android view state needed.
  }

  @Override
  public void scrollToIndex(ShadowlistView view, int index) {
    view.scrollToIndex(index);
  }

  @Override
  public void scrollToOffset(ShadowlistView view, double offset, boolean animated) {
    view.scrollToOffset(offset, animated);
  }

  @Override
  public void scrollToEnd(ShadowlistView view, boolean animated) {
    view.scrollToEnd(animated);
  }

  @Nullable
  @Override
  public Object updateState(
    @NonNull ShadowlistView view,
    @Nullable ReactStylesDiffMap props,
    @Nullable StateWrapper stateWrapper) {
    view.updateState(stateWrapper);
    return super.updateState(view, props, stateWrapper);
  }
}
