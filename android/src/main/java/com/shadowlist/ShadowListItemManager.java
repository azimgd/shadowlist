package com.shadowlist;

import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.shadowlist.ShadowListItemManagerDelegate;
import com.shadowlist.ShadowListItemManagerInterface;

@ReactModule(name = ShadowListItemManager.NAME)
public class ShadowListItemManager extends ViewGroupManager<ShadowListItem> implements ShadowListItemManagerInterface<ShadowListItem> {

  public static final String NAME = "ShadowListItem";

  private final ViewManagerDelegate<ShadowListItem> mDelegate;

  public ShadowListItemManager() {
    mDelegate = new ShadowListItemManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowListItem> getDelegate() {
    return mDelegate;
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public ShadowListItem createViewInstance(ThemedReactContext context) {
    return new ShadowListItem(context);
  }
}
