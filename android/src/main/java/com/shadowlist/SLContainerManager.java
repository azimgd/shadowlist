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

  @ReactProp(name = "inverted")
  @Override
  public void setInverted(SLContainer view, boolean inverted) {
  }

  @ReactProp(name = "horizontal")
  @Override
  public void setHorizontal(SLContainer view, boolean horizontal) {
  }

  @ReactProp(name = "initialNumToRender")
  @Override
  public void setInitialNumToRender(SLContainer view, int initialNumToRender) {
  }

  @ReactProp(name = "initialScrollIndex")
  @Override
  public void setInitialScrollIndex(SLContainer view, int initialScrollIndex) {
  }

  @Nullable
  @Override
  public Object updateState(@NonNull SLContainer view, ReactStylesDiffMap props, StateWrapper stateWrapper) {
    MapBuffer stateMapBuffer = stateWrapper.getStateDataMapBuffer();

    if (stateMapBuffer != null) {
      view.setStateWrapper(stateWrapper);

      view.setScrollContentLayout(
        (float)stateMapBuffer.getDouble(4),
        (float)stateMapBuffer.getDouble(5)
      );

      return super.updateState(view, props, stateWrapper);
    } else {
      return null;
    }
  }

  public static final String NAME = "SLContainer";
}
