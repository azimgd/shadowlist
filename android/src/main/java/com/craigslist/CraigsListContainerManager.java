package com.craigslist;

import android.graphics.Color;

import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.CraigsListContainerManagerDelegate;
import com.facebook.react.viewmanagers.CraigsListContainerManagerInterface;

@ReactModule(name = CraigsListContainerManager.NAME)
public class CraigsListContainerManager extends SimpleViewManager<CraigsListContainer> implements CraigsListContainerManagerInterface<CraigsListContainer> {

  public static final String NAME = "CraigsListContainer";

  private final ViewManagerDelegate<CraigsListContainer> mDelegate;

  public CraigsListContainerManager() {
    mDelegate = new CraigsListContainerManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<CraigsListContainer> getDelegate() {
    return mDelegate;
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public CraigsListContainer createViewInstance(ThemedReactContext context) {
    return new CraigsListContainer(context);
  }

  @Override
  @ReactProp(name = "color")
  public void setColor(CraigsListContainer view, String color) {
    view.setBackgroundColor(Color.parseColor(color));
  }
}
