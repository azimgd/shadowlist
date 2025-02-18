package com.shadowlist;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;

@ReactModule(name = SLContentManager.NAME)
public class SLContentManager extends ViewGroupManager<SLContent>
  implements SLContentManagerInterface<SLContent> {

  private final ViewManagerDelegate<SLContent> mDelegate;

  public SLContentManager() {
    mDelegate = new SLContentManagerDelegate(this);
  }

  @Override
  public ViewManagerDelegate<SLContent> getDelegate() {
    return mDelegate;
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public SLContent createViewInstance(ThemedReactContext context) {
    SLContent view = new SLContent(context);
    return view;
  }

  public static final String NAME = "SLContent";
}
