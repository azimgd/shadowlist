package com.shadowlist;

import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.viewmanagers.SLElementManagerInterface;
import com.facebook.react.viewmanagers.SLElementManagerDelegate;

import java.util.Map;

@ReactModule(name = SLElementManager.NAME)
public class SLElementManager extends ViewGroupManager<SLElement>
  implements SLElementManagerInterface<SLElement> {

  private final ViewManagerDelegate<SLElement> mDelegate;

  public SLElementManager() {
    mDelegate = new SLElementManagerDelegate(this);
  }

  @Override
  public ViewManagerDelegate<SLElement> getDelegate() {
    return mDelegate;
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public SLElement createViewInstance(ThemedReactContext context) {
    SLElement view = new SLElement(context);
    return view;
  }

  public static final String NAME = "SLElement";

  @Override
  public void setIndex(SLElement view, @Nullable int value) {
    view.setIndex(value);
  }

  @Override
  public void setUniqueId(SLElement view, @Nullable String value) {
    view.setUniqueId(value);
  }
}
