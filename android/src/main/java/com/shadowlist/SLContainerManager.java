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

import java.util.Map;

@ReactModule(name = SLContainerManager.NAME)
public class SLContainerManager extends ViewGroupManager<SLContainer>
  implements SLContainerManagerInterface<SLContainer> {

  public static final short SLCONTAINER_STATE_SCROLL_POSITION_LEFT = 2;
  public static final short SLCONTAINER_STATE_SCROLL_POSITION_TOP = 3;
  public static final short SLCONTAINER_STATE_SCROLL_POSITION_UPDATED = 4;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH = 5;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT = 6;
  public static final short SLCONTAINER_STATE_SCROLL_CONTAINER_UPDATED = 7;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH = 8;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT = 9;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_UPDATED = 10;
  public static final short SLCONTAINER_STATE_SCROLL_CONTENT_COMPLETED = 15;
  public static final short SLCONTAINER_STATE_FIRST_CHILD_UNIQUE_ID = 11;
  public static final short SLCONTAINER_STATE_LAST_CHILD_UNIQUE_ID = 12;
  public static final short SLCONTAINER_STATE_SCROLL_INDEX = 13;
  public static final short SLCONTAINER_STATE_SCROLL_INDEX_UPDATED = 14;

  private final ViewManagerDelegate<SLContainer> mDelegate;
  private OnVisibleChangeHandler mVisibleChangeHandler = null;
  private OnStartReachedHandler mStartReachedHandler = null;
  private OnEndReachedHandler mEndReachedHandler = null;
  private OnScrollHandler mScrollHandler = null;

  public interface OnVisibleChangeHandler {
    void onVisibleChange(SLContainer view, int visibleStartIndex, int visibleEndIndex);
  }
  public interface OnEndReachedHandler {
    void onEndReached(SLContainer view, int distanceFromEnd);
  }
  public interface OnStartReachedHandler {
    void onStartReached(SLContainer view, int distanceFromStart);
  }
  public interface OnScrollHandler {
    void onScroll(SLContainer view, int distanceFromStart);
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
    setOnScrollHandler((containerView, distanceFromEnd) ->
      mScrollHandler.onScroll(containerView, distanceFromEnd)
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
    view.setHorizontal(horizontal);
  }

  @ReactProp(name = "initialNumToRender")
  @Override
  public void setInitialNumToRender(SLContainer view, int initialNumToRender) {
  }

  @ReactProp(name = "windowSize")
  @Override
  public void setWindowSize(SLContainer view, int windowSize) {
  }

  @ReactProp(name = "numColumns")
  @Override
  public void setNumColumns(SLContainer view, int numColumns) {
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
      "onEndReached", MapBuilder.of("registrationName", "onEndReached"),
      "onScroll", MapBuilder.of("registrationName", "onScroll")
    );
  }

  private void handleOnVisibleChange(SLContainer view, int visibleStartIndex, int visibleEndIndex) {
    ReactContext reactContext = (ReactContext) view.getContext();
    EventDispatcher eventDispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, view.getId());
    eventDispatcher.dispatchEvent(
      new OnVisibleChangeEvent(UIManagerHelper.getSurfaceId(view), view.getId(), visibleStartIndex, visibleEndIndex)
    );
  }

  private void handleOnStartReached(SLContainer view, int distanceFromStart) {
    ReactContext reactContext = (ReactContext) view.getContext();
    EventDispatcher eventDispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, view.getId());
    eventDispatcher.dispatchEvent(
      new OnStartReachedEvent(UIManagerHelper.getSurfaceId(view), view.getId(), distanceFromStart)
    );
  }

  private void handleOnEndReached(SLContainer view, int distanceFromEnd) {
    ReactContext reactContext = (ReactContext) view.getContext();
    EventDispatcher eventDispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, view.getId());
    eventDispatcher.dispatchEvent(
      new OnEndReachedEvent(UIManagerHelper.getSurfaceId(view), view.getId(), distanceFromEnd)
    );
  }

  private void handleOnScroll(SLContainer view, int scrollContentWidth, int scrollContentHeight, int scrollPositionLeft, int scrollPositionTop) {
    ReactContext reactContext = (ReactContext) view.getContext();
    EventDispatcher eventDispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, view.getId());
    eventDispatcher.dispatchEvent(
      new OnScrollEvent(UIManagerHelper.getSurfaceId(view), view.getId(), scrollPositionLeft, scrollPositionTop, scrollContentWidth, scrollContentHeight)
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

  public void setOnScrollHandler(OnScrollHandler handler) {
    mScrollHandler = handler;
  }

  @Nullable
  @Override
  public Object updateState(@NonNull SLContainer view, ReactStylesDiffMap props, StateWrapper stateWrapper) {
    MapBuffer stateMapBuffer = stateWrapper.getStateDataMapBuffer();

    if (stateMapBuffer != null) {
      view.setStateWrapper(stateWrapper, this::handleOnStartReached, this::handleOnEndReached, this::handleOnVisibleChange);
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
