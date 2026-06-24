package com.shadowlist;

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

  @Override
  @ReactProp(name = "templateType")
  public void setTemplateType(ShadowListTemplateView view, String templateType) {
    view.setTemplateType(templateType);
  }
}
