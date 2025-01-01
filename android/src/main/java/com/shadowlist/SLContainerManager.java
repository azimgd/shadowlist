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

  public static final short SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE = 0;
  public static final short SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE = 1;
  public static final short SLCONTAINER_STATE_SCROLL_POSITION_LEFT = 4;
  public static final short SLCONTAINER_STATE_SCROLL_POSITION_TOP = 5;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH = 6;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT = 7;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH = 8;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT = 9;

  private final ViewManagerDelegate<SLContainer> mDelegate;
  private OnVisibleChangeHandler mVisibleChangeHandler = null;
  private OnStartReachedHandler mStartReachedHandler = null;
  private OnEndReachedHandler mEndReachedHandler = null;

  public interface OnVisibleChangeHandler {
    void onVisibleChange(SLContainer view, int visibleStartIndex, int visibleEndIndex);
  }
  public interface OnEndReachedHandler {
    void onEndReached(SLContainer view, int distanceFromEnd);
  }
  public interface OnStartReachedHandler {
    void onStartReached(SLContainer view, int distanceFromStart);
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
    setOnStartReachedHandler((containerView, distanceFromStart) ->
      mStartReachedHandler.onStartReached(containerView, distanceFromStart)
    );
    setOnEndReachedHandler((containerView, distanceFromEnd) ->
      mEndReachedHandler.onEndReached(containerView, distanceFromEnd)
    );
  }

  @Override
  public SLContainer createViewInstance(ThemedReactContext context) {
    SLContainer view = new SLContainer(context);
    return view;
  }

  @ReactProp(name = "data")
  @Override
  public void setData(SLContainer view, String data) {
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
      "onVisibleChange", MapBuilder.of("registrationName", "onVisibleChange"),
      "onStartReached", MapBuilder.of("registrationName", "onStartReached"),
      "onEndReached", MapBuilder.of("registrationName", "onEndReached")
    );
  }

  private void handleOnVisibleChange(SLContainer view, int visibleStartIndex, int visibleEndIndex) {
    ReactContext reactContext = (ReactContext) view.getContext();
    EventDispatcher eventDispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, view.getId());
    eventDispatcher.dispatchEvent(
      new OnVisibleChangeEvent(UIManagerHelper.getSurfaceId(view), view.getId(), visibleStartIndex, visibleEndIndex)
    );
  }

  private void handleStartReached(SLContainer view, int distanceFromStart) {
    ReactContext reactContext = (ReactContext) view.getContext();
    EventDispatcher eventDispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, view.getId());
    eventDispatcher.dispatchEvent(
      new OnStartReachedEvent(UIManagerHelper.getSurfaceId(view), view.getId(), distanceFromStart)
    );
  }

  private void handleEndReached(SLContainer view, int distanceFromEnd) {
    ReactContext reactContext = (ReactContext) view.getContext();
    EventDispatcher eventDispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, view.getId());
    eventDispatcher.dispatchEvent(
      new OnEndReachedEvent(UIManagerHelper.getSurfaceId(view), view.getId(), distanceFromEnd)
    );
  }

  public void setOnVisibleChangeHandler(OnVisibleChangeHandler handler) {
    mVisibleChangeHandler = handler;
  }

  public void setOnStartReachedHandler(OnStartReachedHandler handler) {
    mStartReachedHandler = handler;
  }

  public void setOnEndReachedHandler(OnEndReachedHandler handler) {
    mEndReachedHandler = handler;
  }

  @Nullable
  @Override
  public Object updateState(@NonNull SLContainer view, ReactStylesDiffMap props, StateWrapper stateWrapper) {
    MapBuffer stateMapBuffer = stateWrapper.getStateDataMapBuffer();

    if (stateMapBuffer != null) {
      view.setStateWrapper(stateWrapper, this::handleStartReached, this::handleEndReached, this::handleOnVisibleChange);
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
    view.scrollToIndex(index, animated);
  }

  @Override
  public void scrollToOffset(SLContainer view, int offset, boolean animated) {
    view.scrollToOffset(offset, animated);
  }

  public static final String NAME = "SLContainer";
}
