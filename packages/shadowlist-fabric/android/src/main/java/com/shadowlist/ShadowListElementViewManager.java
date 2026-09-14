package com.shadowlist;

import android.view.View;

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

  /*
   * Recycled row views are pooled per surface and handed to ANY ShadowList in that surface,
   * so with a single-surface navigator that means a list on a different screen. The base
   * implementation clears translationX/Y, elevation and alpha, but not translationZ or
   * visibility -- both of which drag-to-reorder writes (ShadowListDragController lifts the
   * picked-up row in Z and shifts its siblings). A row unmounted mid-drag would otherwise
   * return to the pool displaced in Z and reappear floating above another list's content.
   */
  @Nullable
  @Override
  protected ShadowListElementView prepareToRecycleView(
      @NonNull ThemedReactContext reactContext, @NonNull ShadowListElementView view) {
    view.setTranslationZ(0f);
    view.setVisibility(View.VISIBLE);
    return super.prepareToRecycleView(reactContext, view);
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
