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
   * Recycled rows can end up in any list on the surface, even one on another screen.
   * The base class does not reset translationZ or visibility, and drag to reorder sets both.
   * Reset them so a row unmounted mid drag doesn't float above another list.
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
    view.setElementIndex(index);
  }

  @Override
  @ReactProp(name = "elementKey")
  public void setElementKey(ShadowListElementView view, @Nullable String value) {
    // Drag to reorder reads this to find the key of the touched row.
    view.setElementKey(value);
  }
}
