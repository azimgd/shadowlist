package com.shadowlist;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.ShadowListElementViewManagerInterface;
import com.facebook.react.viewmanagers.ShadowListElementViewManagerDelegate;

@ReactModule(name = ShadowListElementViewManager.NAME)
public class ShadowListElementViewManager extends ViewGroupManager<ShadowListElementView>
    implements ShadowListElementViewManagerInterface<ShadowListElementView> {

  public static final String NAME = "ShadowListElementView";

  private final ViewManagerDelegate<ShadowListElementView> mDelegate;

  public ShadowListElementViewManager() {
    mDelegate = new ShadowListElementViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowListElementView> getDelegate() {
    return mDelegate;
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  protected ShadowListElementView createViewInstance(@NonNull ThemedReactContext context) {
    return new ShadowListElementView(context);
  }

  @Override
  @ReactProp(name = "index")
  public void setIndex(ShadowListElementView view, int index) {
    // Mirrored onto the view as a debug/fallback handle.
    view.setElementIndex(index);
  }

  @Override
  @ReactProp(name = "elementKey")
  public void setElementKey(ShadowListElementView view, @Nullable String value) {
    // Mirrored onto the view so drag-to-reorder can map a touched child to its key.
    view.setElementKey(value);
  }
}
