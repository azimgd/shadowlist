package com.shadowlist;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.common.MapBuilder;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ReactStylesDiffMap;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.common.mapbuffer.MapBuffer;
import com.facebook.react.uimanager.events.EventDispatcher;
import com.facebook.react.viewmanagers.SLContainerManagerInterface;
import com.facebook.react.viewmanagers.SLContainerManagerDelegate;

import java.util.Map;

@ReactModule(name = SLContainerManager.NAME)
public class SLContainerManager extends ViewGroupManager<SLContainer>
  implements SLContainerManagerInterface<SLContainer> {

  public static final short SLCONTAINER_STATE_VISIBLE_START_INDEX = 0;
  public static final short SLCONTAINER_STATE_VISIBLE_END_INDEX = 1;
  public static final short SLCONTAINER_STATE_VISIBLE_START_TRIGGER = 2;
  public static final short SLCONTAINER_STATE_VISIBLE_END_TRIGGER = 3;
  public static final short SLCONTAINER_STATE_SCROLL_POSITION_LEFT = 4;
  public static final short SLCONTAINER_STATE_SCROLL_POSITION_TOP = 5;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH = 6;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT = 7;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH = 8;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT = 9;
  public static final short SLCONTAINER_STATE_HORIZONTAL = 10;
  public static final short SLCONTAINER_STATE_INITIAL_NUM_TO_RENDER = 11;

  private final ViewManagerDelegate<SLContainer> mDelegate;
  private OnVisibleChangeHandler mVisibleChangeHandler = null;

  public interface OnVisibleChangeHandler {
    void onVisibleChange(SLContainer view, int visibleStartIndex, int visibleEndIndex);
  }

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
  protected void addEventEmitters(@NonNull ThemedReactContext reactContext, @NonNull SLContainer view) {
    super.addEventEmitters(reactContext, view);
    setOnVisibleChangeHandler((containerView, visibleStartIndex, visibleEndIndex) ->
      mVisibleChangeHandler.onVisibleChange(containerView, visibleStartIndex, visibleEndIndex)
    );
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
  public Map getExportedCustomDirectEventTypeConstants() {
    return MapBuilder.of(
      "onVisibleChange", MapBuilder.of("registrationName", "onVisibleChange")
    );
  }

  private void handleOnVisibleChange(SLContainer view, int visibleStartIndex, int visibleEndIndex) {
    ReactContext reactContext = (ReactContext) view.getContext();
    EventDispatcher eventDispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, view.getId());
    eventDispatcher.dispatchEvent(
      new OnVisibleChangeEvent(UIManagerHelper.getSurfaceId(view), view.getId(), visibleStartIndex, visibleEndIndex)
    );
  }

  public void setOnVisibleChangeHandler(OnVisibleChangeHandler handler) {
    mVisibleChangeHandler = handler;
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

      view.setScrollContainerOffset(
        (int)stateMapBuffer.getDouble(SLCONTAINER_STATE_SCROLL_POSITION_LEFT),
        (int)stateMapBuffer.getDouble(SLCONTAINER_STATE_SCROLL_POSITION_TOP)
      );

      int visibleStartIndex = stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_VISIBLE_START_INDEX);
      int visibleEndIndex = stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_VISIBLE_END_INDEX) == 0 ?
        stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_INITIAL_NUM_TO_RENDER) :
        stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_VISIBLE_END_INDEX);

      handleOnVisibleChange(
        view,
        visibleStartIndex,
        visibleEndIndex
      );

      return super.updateState(view, props, stateWrapper);
    } else {
      return null;
    }
  }

  @Override
  public void receiveCommand(@NonNull SLContainer view, String commandId, @Nullable ReadableArray args) {
    if (commandId.equals("scrollToIndex")) {
      scrollToIndex(view, args.getInt(0), args.getBoolean(1));
    } else if (commandId.equals("scrollToOffset")) {
      scrollToOffset(view, args.getInt(0), args.getBoolean(1));
    }
  }

  @Override
  public void scrollToIndex(SLContainer view, int index, boolean animated) {

  }

  @Override
  public void scrollToOffset(SLContainer view, int offset, boolean animated) {

  }

  public static final String NAME = "SLContainer";
}
