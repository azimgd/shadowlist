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
import com.facebook.react.viewmanagers.ShadowListTemplateViewManagerInterface;
import com.facebook.react.viewmanagers.ShadowListTemplateViewManagerDelegate;

@ReactModule(name = ShadowListTemplateViewManager.NAME)
public class ShadowListTemplateViewManager extends ViewGroupManager<ShadowListTemplateView>
    implements ShadowListTemplateViewManagerInterface<ShadowListTemplateView> {

  public static final String NAME = "ShadowListTemplateView";

  private final ViewManagerDelegate<ShadowListTemplateView> mDelegate;

  public ShadowListTemplateViewManager() {
    mDelegate = new ShadowListTemplateViewManagerDelegate(this);
    // Only takes effect when the enableViewRecycling feature flag is on.
    setupViewRecycling();
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowListTemplateView> getDelegate() {
    return mDelegate;
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  protected ShadowListTemplateView createViewInstance(@NonNull ThemedReactContext context) {
    return new ShadowListTemplateView(context);
  }

  /*
   * Recycled templates can end up in any list on the surface, even one on another screen.
   * The base class resets transforms and alpha but not translationZ, visibility, children,
   * background or the list's listeners, and the sticky controller sets them. A hidden
   * overlay reused as another list's header would leave an empty gap.
   */
  @Nullable
  @Override
  protected ShadowListTemplateView prepareToRecycleView(
      @NonNull ThemedReactContext reactContext, @NonNull ShadowListTemplateView view) {
    view.animate().cancel();
    view.clearAnimation();
    view.setTranslationZ(0f);
    view.setVisibility(View.VISIBLE);
    ShadowListTemplateView prepared = super.prepareToRecycleView(reactContext, view);
    if (prepared == null) {
      return null;
    }
    prepared.resetForRecycle();
    BackgroundStyleApplicator.reset(prepared);
    return prepared;
  }

  @Override
  @ReactProp(name = "templateType")
  public void setTemplateType(ShadowListTemplateView view, String templateType) {
    view.setTemplateType(templateType);
  }
}
