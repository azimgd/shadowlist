package com.shadowlist;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.ShadowlistElementViewManagerInterface;
import com.facebook.react.viewmanagers.ShadowlistElementViewManagerDelegate;

@ReactModule(name = ShadowlistElementViewManager.NAME)
public class ShadowlistElementViewManager extends ViewGroupManager<ShadowlistElementView>
    implements ShadowlistElementViewManagerInterface<ShadowlistElementView> {

  public static final String NAME = "ShadowlistElementView";

  private final ViewManagerDelegate<ShadowlistElementView> mDelegate;

  public ShadowlistElementViewManager() {
    mDelegate = new ShadowlistElementViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowlistElementView> getDelegate() {
    return mDelegate;
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  protected ShadowlistElementView createViewInstance(@NonNull ThemedReactContext context) {
    return new ShadowlistElementView(context);
  }

  @Override
  @ReactProp(name = "index")
  public void setIndex(ShadowlistElementView view, int index) {
  }
}
