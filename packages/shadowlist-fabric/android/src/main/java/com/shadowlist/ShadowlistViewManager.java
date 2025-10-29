package com.shadowlist;

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

  @Override
  @ReactProp(name = "elementsAllKeys")
  public void setElementsAllKeys(ShadowlistView view, @Nullable ReadableArray value) {

  }

  @Override
  @ReactProp(name = "elementsHeadKey")
  public void setElementsHeadKey(ShadowlistView view, @Nullable String value) {

  }

  @Override
  @ReactProp(name = "elementsTailKey")
  public void setElementsTailKey(ShadowlistView view, @Nullable String value) {

  }

  @Override
  @ReactProp(name = "inverted")
  public void setInverted(ShadowlistView view, boolean value) {

  }

  @Override
  @ReactProp(name = "horizontal")
  public void setHorizontal(ShadowlistView view, boolean value) {

  }

  @Override
  public void prependElements(ShadowlistView view, int size) {

  }

  @Override
  public void appendElements(ShadowlistView view, int size) {

  }

  @Nullable
  @Override
  public Object updateState(
      @NonNull ShadowlistView view,
      @Nullable ReactStylesDiffMap props,
      @Nullable StateWrapper stateWrapper) {
    return super.updateState(view, props, stateWrapper);
  }
}
