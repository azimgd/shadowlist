package com.shadowlist;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.common.MapBuilder;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.BackgroundStyleApplicator;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.ShadowListElementViewManagerInterface;
import com.facebook.react.viewmanagers.ShadowListElementViewManagerDelegate;

import java.util.Map;

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

  @Nullable
  @Override
  public Map<String, Object> getExportedCustomDirectEventTypeConstants() {
    // Hook the events a row sends itself up to their JS handlers.
    return MapBuilder.<String, Object>builder()
      .put(
        ShadowListActionEvent.SWIPE_ACTION,
        MapBuilder.of("registrationName", "onSwipeAction"))
      .put(
        ShadowListActionEvent.CONTEXT_MENU_ACTION,
        MapBuilder.of("registrationName", "onContextMenuAction"))
      .build();
  }

  /*
   * Recycled rows can end up in any list on the surface, even one on another screen.
   * The base class resets transforms, alpha and elevation but not translationZ, visibility,
   * running animations, children or background. Drag to reorder sets translationZ and runs
   * a drop animation. A swipe moves and animates the row's drawing. Reset all of it so a
   * reused row starts like a new one.
   */
  @Nullable
  @Override
  protected ShadowListElementView prepareToRecycleView(
      @NonNull ThemedReactContext reactContext, @NonNull ShadowListElementView view) {
    if (!detachForRecycle(view)) {
      return null;
    }
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

  /*
   * Take a dropped view out of its parent before it goes into the pool, and report whether
   * it left. A view deleted with a screen that animates out is still drawn by its previous parent
   * until the animation ends: the screen container started a view transition on it.
   * removeView only marks it disappearing and getParent() stays set. Reusing it would fail
   * in addView with "the specified child already has a parent". It is not recycled.
   */
  static boolean detachForRecycle(View view) {
    ViewParent parent = view.getParent();
    if (parent instanceof ViewGroup) {
      ((ViewGroup) parent).removeView(view);
    }
    return view.getParent() == null;
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

  @Override
  @ReactProp(name = "leadingSwipeActions")
  public void setLeadingSwipeActions(ShadowListElementView view, @Nullable ReadableArray value) {
    view.setLeadingSwipeActions(value);
  }

  @Override
  @ReactProp(name = "trailingSwipeActions")
  public void setTrailingSwipeActions(ShadowListElementView view, @Nullable ReadableArray value) {
    view.setTrailingSwipeActions(value);
  }

  @Override
  public void closeFullSwipe(ShadowListElementView view) {
    view.closeFullSwipe();
  }

  @Override
  @ReactProp(name = "leadingFullSwipe", defaultBoolean = true)
  public void setLeadingFullSwipe(ShadowListElementView view, boolean value) {
    view.setLeadingFullSwipe(value);
  }

  @Override
  @ReactProp(name = "trailingFullSwipe", defaultBoolean = true)
  public void setTrailingFullSwipe(ShadowListElementView view, boolean value) {
    view.setTrailingFullSwipe(value);
  }

  @Override
  @ReactProp(name = "contextMenuTitle")
  public void setContextMenuTitle(ShadowListElementView view, @Nullable String value) {
    view.setContextMenuTitle(value);
  }

  @Override
  @ReactProp(name = "contextMenuActions")
  public void setContextMenuActions(ShadowListElementView view, @Nullable ReadableArray value) {
    view.setContextMenuActions(value);
  }
}
