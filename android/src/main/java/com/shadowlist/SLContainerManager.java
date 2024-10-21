package com.shadowlist;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ReactStylesDiffMap;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.common.mapbuffer.MapBuffer;
import com.facebook.react.viewmanagers.SLContainerManagerInterface;
import com.facebook.react.viewmanagers.SLContainerManagerDelegate;

@ReactModule(name = SLContainerManager.NAME)
public class SLContainerManager extends ViewGroupManager<SLContainer>
  implements SLContainerManagerInterface<SLContainer> {

  private static final short CX_STATE_KEY_CHILDREN_MEASUREMENTS = 0;

  private final ViewManagerDelegate<SLContainer> mDelegate;

  public SLContainerManager() {
    mDelegate = new SLContainerManagerDelegate(this);
  }

  @Override
  public ViewManagerDelegate<SLContainer> getDelegate() {
    return mDelegate;
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public SLContainer createViewInstance(ThemedReactContext context) {
    return new SLContainer(context);
  }

  @ReactProp(name = "color")
  @Override
  public void setColor(SLContainer view, String color) {
  }

  @Nullable
  @Override
  public Object updateState(@NonNull SLContainer view, ReactStylesDiffMap props, StateWrapper stateWrapper) {
    MapBuffer stateMapBuffer = stateWrapper.getStateDataMapBuffer();

    if (stateMapBuffer != null) {
      return stateMapBuffer;
    } else {
      return null;
    }
  }


  public static final String NAME = "SLContainer";
}
