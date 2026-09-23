package com.shadowlist;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.BackgroundStyleApplicator;
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
    // Only takes effect when the enableViewRecycling feature flag is on.
    setupViewRecycling();
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
   * The base class resets transforms, alpha and elevation but not translationZ, visibility,
   * running animations, children or background. Drag to reorder sets translationZ and runs
   * a drop animation. Reset all of it so a reused row starts like a new one.
   */
  @Nullable
  @Override
  protected ShadowListElementView prepareToRecycleView(
      @NonNull ThemedReactContext reactContext, @NonNull ShadowListElementView view) {
    // Stop a drop animation first, or it keeps writing translation after the reset.
    view.animate().cancel();
    view.clearAnimation();
    view.setTranslationZ(0f);
    view.setVisibility(View.VISIBLE);
    ShadowListElementView prepared = super.prepareToRecycleView(reactContext, view);
    if (prepared == null) {
      return null;
    }
    prepared.resetForRecycle();
    BackgroundStyleApplicator.reset(prepared);
    return prepared;
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
