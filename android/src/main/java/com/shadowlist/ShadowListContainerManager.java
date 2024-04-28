package com.shadowlist;

import android.graphics.Color;

import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.ShadowListContainerManagerDelegate;
import com.facebook.react.viewmanagers.ShadowListContainerManagerInterface;

@ReactModule(name = ShadowListContainerManager.NAME)
public class ShadowListContainerManager extends SimpleViewManager<ShadowListContainer> implements ShadowListContainerManagerInterface<ShadowListContainer> {

  public static final String NAME = "ShadowListContainer";

  private final ViewManagerDelegate<ShadowListContainer> mDelegate;

  public ShadowListContainerManager() {
    mDelegate = new ShadowListContainerManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowListContainer> getDelegate() {
    return mDelegate;
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public ShadowListContainer createViewInstance(ThemedReactContext context) {
    return new ShadowListContainer(context);
  }

  @Override
  @ReactProp(name = "color")
  public void setColor(ShadowListContainer view, String color) {
    view.setBackgroundColor(Color.parseColor(color));
  }
}
