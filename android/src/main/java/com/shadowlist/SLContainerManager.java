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

  public static final short SLCONTAINER_STATE_VISIBLE_START_INDEX = 0;
  public static final short SLCONTAINER_STATE_VISIBLE_END_INDEX = 1;
  public static final short SLCONTAINER_STATE_SCROLL_POSITION_LEFT = 2;
  public static final short SLCONTAINER_STATE_SCROLL_POSITION_TOP = 3;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH = 4;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT = 5;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH = 6;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT = 7;
  public static final short SLCONTAINER_STATE_HORIZONTAL = 8;
  public static final short SLCONTAINER_STATE_INITIAL_NUM_TO_RENDER = 9;

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
    SLContainer view = new SLContainer(context);
    return view;
  }

  @ReactProp(name = "inverted")
  @Override
  public void setInverted(SLContainer view, boolean inverted) {
  }

  @ReactProp(name = "horizontal")
  @Override
  public void setHorizontal(SLContainer view, boolean horizontal) {
    if (horizontal) {
      view.setScrollContainerHorizontal();
    } else {
      view.setScrollContainerVertical();
    }
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

      view.setScrollContainerLayout(
        (int)stateMapBuffer.getDouble(SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH),
        (int)stateMapBuffer.getDouble(SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT)
      );

      view.setScrollContentLayout(
        (int)stateMapBuffer.getDouble(SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH),
        (int)stateMapBuffer.getDouble(SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT)
      );

      return super.updateState(view, props, stateWrapper);
    } else {
      return null;
    }
  }

  public static final String NAME = "SLContainer";
}
