package com.shadowlist;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.ShadowlistTemplateViewManagerInterface;
import com.facebook.react.viewmanagers.ShadowlistTemplateViewManagerDelegate;

@ReactModule(name = ShadowlistTemplateViewManager.NAME)
public class ShadowlistTemplateViewManager extends ViewGroupManager<ShadowlistTemplateView>
    implements ShadowlistTemplateViewManagerInterface<ShadowlistTemplateView> {

  public static final String NAME = "ShadowlistTemplateView";

  private final ViewManagerDelegate<ShadowlistTemplateView> mDelegate;

  public ShadowlistTemplateViewManager() {
    mDelegate = new ShadowlistTemplateViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowlistTemplateView> getDelegate() {
    return mDelegate;
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  protected ShadowlistTemplateView createViewInstance(@NonNull ThemedReactContext context) {
    return new ShadowlistTemplateView(context);
  }

  @Override
  @ReactProp(name = "templateType")
  public void setTemplateType(ShadowlistTemplateView view, String templateType) {
    view.setTemplateType(templateType);
  }
}
