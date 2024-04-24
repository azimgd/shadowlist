package com.craigslist;

import android.graphics.Color;

import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.CraigsListViewManagerDelegate;
import com.facebook.react.viewmanagers.CraigsListViewManagerInterface;

@ReactModule(name = CraigsListViewManager.NAME)
public class CraigsListViewManager extends SimpleViewManager<CraigsListView> implements CraigsListViewManagerInterface<CraigsListView> {

  public static final String NAME = "CraigsListView";

  private final ViewManagerDelegate<CraigsListView> mDelegate;

  public CraigsListViewManager() {
    mDelegate = new CraigsListViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<CraigsListView> getDelegate() {
    return mDelegate;
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public CraigsListView createViewInstance(ThemedReactContext context) {
    return new CraigsListView(context);
  }

  @Override
  @ReactProp(name = "color")
  public void setColor(CraigsListView view, String color) {
    view.setBackgroundColor(Color.parseColor(color));
  }
}
