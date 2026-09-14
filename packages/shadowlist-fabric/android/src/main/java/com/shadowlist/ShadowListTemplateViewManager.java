package com.shadowlist;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
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
   * Recycled template views are pooled per surface and handed to ANY ShadowList in that
   * surface, so with a single-surface navigator that means a list on a different screen. The
   * base implementation clears translationX/Y, elevation and alpha, but not translationZ or
   * visibility -- and ShadowListStickyController sets both, hiding the section-header
   * overlay outright (View.GONE) whenever no section is active and lifting pinned views in Z.
   *
   * A GONE overlay recycled as another list's header stays invisible while the shadow node
   * keeps reserving its measured size, which reads as a freshly opened list laid out around
   * a header that is not there.
   */
  @Nullable
  @Override
  protected ShadowListTemplateView prepareToRecycleView(
      @NonNull ThemedReactContext reactContext, @NonNull ShadowListTemplateView view) {
    view.setTranslationZ(0f);
    view.setVisibility(View.VISIBLE);
    return super.prepareToRecycleView(reactContext, view);
  }

  @Override
  @ReactProp(name = "templateType")
  public void setTemplateType(ShadowListTemplateView view, String templateType) {
    view.setTemplateType(templateType);
  }
}
