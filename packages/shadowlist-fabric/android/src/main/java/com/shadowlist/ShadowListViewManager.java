package com.shadowlist;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.common.MapBuilder;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ReactStylesDiffMap;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.uimanager.ViewGroupManager;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.uimanager.annotations.ReactProp;
import com.facebook.react.viewmanagers.ShadowListViewManagerInterface;
import com.facebook.react.viewmanagers.ShadowListViewManagerDelegate;

import java.util.Map;

@ReactModule(name = ShadowListViewManager.NAME)
public class ShadowListViewManager extends ViewGroupManager<ShadowListView>
  implements ShadowListViewManagerInterface<ShadowListView> {

  public static final String NAME = "ShadowListView";

  private final ViewManagerDelegate<ShadowListView> mDelegate;

  public ShadowListViewManager() {
    mDelegate = new ShadowListViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<ShadowListView> getDelegate() {
    return mDelegate;
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  protected ShadowListView createViewInstance(@NonNull ThemedReactContext context) {
    return new ShadowListView(context);
  }

  @Override
  public void onDropViewInstance(@NonNull ShadowListView view) {
    // Stop any drag before the view is recycled.
    view.onDropInstance();
    super.onDropViewInstance(view);
  }

  /*
   * Children mount into the content view inside the scroll view.
   */
  @Override
  public void addView(ShadowListView parent, View child, int index) {
    parent.addContentView(child, index);
  }

  @Override
  public int getChildCount(ShadowListView parent) {
    return parent.getContentChildCount();
  }

  @Override
  public View getChildAt(ShadowListView parent, int index) {
    return parent.getContentChildAt(index);
  }

  @Override
  public void removeViewAt(ShadowListView parent, int index) {
    parent.removeContentViewAt(index);
  }

  @Override
  @ReactProp(name = "elementsAllKeys")
  public void setElementsAllKeys(ShadowListView view, @Nullable ReadableArray elementsAllKeys) {
    // The core reads this prop. The view only counts rows for accessibility.
    view.setItemKeys(elementsAllKeys);
  }

  @Override
  @ReactProp(name = "elementsAnchorIgnoreKeys")
  public void setElementsAnchorIgnoreKeys(
      ShadowListView view, @Nullable ReadableArray elementsAnchorIgnoreKeys) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "elementsSizeSpecs")
  public void setElementsSizeSpecs(
      ShadowListView view, @Nullable String elementsSizeSpecs) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "inverted")
  public void setInverted(ShadowListView view, boolean inverted) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "followAppends")
  public void setFollowAppends(ShadowListView view, boolean followAppends) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "horizontal")
  public void setHorizontal(ShadowListView view, boolean horizontal) {
    view.setHorizontal(horizontal);
  }

  @Override
  @ReactProp(name = "stickyHeader")
  public void setStickyHeader(ShadowListView view, boolean stickyHeader) {
    view.setStickyHeader(stickyHeader);
  }

  @Override
  @ReactProp(name = "stickyFooter")
  public void setStickyFooter(ShadowListView view, boolean stickyFooter) {
    view.setStickyFooter(stickyFooter);
  }

  @Override
  @ReactProp(name = "autoHideHeader")
  public void setAutoHideHeader(ShadowListView view, boolean autoHideHeader) {
    view.setAutoHideHeader(autoHideHeader);
  }

  @Override
  @ReactProp(name = "autoHideFooter")
  public void setAutoHideFooter(ShadowListView view, boolean autoHideFooter) {
    view.setAutoHideFooter(autoHideFooter);
  }

  @Override
  @ReactProp(name = "reorderEnabled")
  public void setReorderEnabled(ShadowListView view, boolean reorderEnabled) {
    view.setReorderEnabled(reorderEnabled);
  }

  @Override
  @ReactProp(name = "stickyIndices")
  public void setStickyIndices(ShadowListView view, @Nullable ReadableArray stickyIndices) {
    // Only the core reads this prop. Pinning reads the header positions back from state.
  }

  @Override
  public void setStartReachedEnabled(ShadowListView view, boolean enabled) {
    view.setStartReachedEnabled(enabled);
  }

  @Override
  public void setEndReachedEnabled(ShadowListView view, boolean enabled) {
    view.setEndReachedEnabled(enabled);
  }

  @Override
  @ReactProp(name = "numberOfColumns")
  public void setNumberOfColumns(ShadowListView view, int numberOfColumns) {
    // The core lays the columns out. The view only needs them for grid drags.
    view.setNumberOfColumns(numberOfColumns);
  }

  @Override
  @ReactProp(name = "containerOffsetIndex")
  public void setContainerOffsetIndex(ShadowListView view, int containerOffsetIndex) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "refreshEnabled")
  public void setRefreshEnabled(ShadowListView view, boolean refreshEnabled) {
    view.setRefreshEnabled(refreshEnabled);
  }

  @Override
  @ReactProp(name = "refreshing")
  public void setRefreshing(ShadowListView view, boolean refreshing) {
    view.setRefreshing(refreshing);
  }

  @Override
  @ReactProp(name = "refreshColor", customType = "Color")
  public void setRefreshColor(ShadowListView view, @Nullable Integer refreshColor) {
    view.setRefreshColor(refreshColor);
  }

  @Nullable
  @Override
  public Map<String, Object> getExportedCustomDirectEventTypeConstants() {
    // Hook the events this view sends itself up to their JS handlers.
    return MapBuilder.<String, Object>builder()
      .put(
        ShadowListRefreshEvent.EVENT_NAME,
        MapBuilder.of("registrationName", "onRefresh"))
      .put(
        ShadowListRefreshEvent.SETTLE_EVENT_NAME,
        MapBuilder.of("registrationName", "onRefreshSettle"))
      .put(
        ShadowListScrollEvent.BEGIN_DRAG,
        MapBuilder.of("registrationName", "onScrollBeginDrag"))
      .put(
        ShadowListScrollEvent.END_DRAG,
        MapBuilder.of("registrationName", "onScrollEndDrag"))
      .put(
        ShadowListScrollEvent.MOMENTUM_BEGIN,
        MapBuilder.of("registrationName", "onMomentumScrollBegin"))
      .put(
        ShadowListScrollEvent.MOMENTUM_END,
        MapBuilder.of("registrationName", "onMomentumScrollEnd"))
      .build();
  }

  @Override
  @ReactProp(name = "startReachedThreshold")
  public void setStartReachedThreshold(ShadowListView view, double startReachedThreshold) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "endReachedThreshold")
  public void setEndReachedThreshold(ShadowListView view, double endReachedThreshold) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "viewableRules")
  public void setViewableRules(ShadowListView view, @Nullable ReadableArray viewableRules) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "scrollEventThrottle")
  public void setScrollEventThrottle(ShadowListView view, double scrollEventThrottle) {
    // Only the component descriptor reads this prop.
  }

  @Override
  @ReactProp(name = "contentSizeEventEnabled")
  public void setContentSizeEventEnabled(ShadowListView view, boolean contentSizeEventEnabled) {
    // Only the layout pass reads this prop.
  }

  @Override
  @ReactProp(name = "scrollEnabled", defaultBoolean = true)
  public void setScrollEnabled(ShadowListView view, boolean scrollEnabled) {
    view.setScrollEnabled(scrollEnabled);
  }

  @Override
  @ReactProp(name = "showsVerticalScrollIndicator", defaultBoolean = true)
  public void setShowsVerticalScrollIndicator(ShadowListView view, boolean shows) {
    view.setShowsVerticalScrollIndicator(shows);
  }

  @Override
  @ReactProp(name = "showsHorizontalScrollIndicator", defaultBoolean = true)
  public void setShowsHorizontalScrollIndicator(ShadowListView view, boolean shows) {
    view.setShowsHorizontalScrollIndicator(shows);
  }

  @Override
  @ReactProp(name = "bounces", defaultBoolean = true)
  public void setBounces(ShadowListView view, boolean bounces) {
    view.setBounces(bounces);
  }

  @Override
  @ReactProp(name = "decelerationRate")
  public void setDecelerationRate(ShadowListView view, double decelerationRate) {
    view.setDecelerationRate(decelerationRate);
  }

  @Override
  @ReactProp(name = "scrollsToTop", defaultBoolean = true)
  public void setScrollsToTop(ShadowListView view, boolean scrollsToTop) {
    // Android has no status bar tap.
  }

  @Override
  @ReactProp(name = "keyboardDismissMode")
  public void setKeyboardDismissMode(ShadowListView view, @Nullable String keyboardDismissMode) {
    // JS dismisses the keyboard when a drag begins, like React Native's ScrollView on Android.
  }

  @Override
  @ReactProp(name = "nestedScrollEnabled")
  public void setNestedScrollEnabled(ShadowListView view, boolean nestedScrollEnabled) {
    view.setNestedScrollEnabled(nestedScrollEnabled);
  }

  @Override
  @ReactProp(name = "refreshProgressViewOffset")
  public void setRefreshProgressViewOffset(ShadowListView view, double refreshProgressViewOffset) {
    view.setRefreshProgressViewOffset(refreshProgressViewOffset);
  }

  @Override
  @ReactProp(name = "overscan")
  public void setOverscan(ShadowListView view, double overscan) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "scrollEventEnabled")
  public void setScrollEventEnabled(ShadowListView view, boolean scrollEventEnabled) {
    // The component descriptor reads this to decide whether to watch scrolls at all.
  }

  @Override
  @ReactProp(name = "viewableEventEnabled")
  public void setViewableEventEnabled(ShadowListView view, boolean viewableEventEnabled) {
    // Same, for watching which rows are visible.
  }

  @Override
  @ReactProp(name = "snapToItem")
  public void setSnapToItem(ShadowListView view, boolean snapToItem) {
    view.setSnapToItem(snapToItem);
  }

  @Override
  @ReactProp(name = "snapAlignment")
  public void setSnapAlignment(ShadowListView view, int snapAlignment) {
    // The core applies the alignment. The view only needs the snap offsets.
  }

  @Override
  public void scrollToItem(ShadowListView view, int index, double viewPosition, double viewOffset, boolean animated) {
    view.scrollToItem(index, viewPosition, viewOffset, animated);
  }

  @Override
  public void scrollToOffset(ShadowListView view, double offset, boolean animated) {
    view.scrollToOffset(offset, animated);
  }

  @Override
  public void scrollToEnd(ShadowListView view, boolean animated) {
    view.scrollToEnd(animated);
  }

  @Override
  public void flashScrollIndicators(ShadowListView view) {
    view.flashScrollIndicators();
  }

  @Override
  public void requestAnchorState(ShadowListView view) {
    view.requestAnchorState();
  }

  @Override
  public void closeSwipeActions(ShadowListView view) {
    view.closeSwipeActions();
  }

  @Nullable
  @Override
  public Object updateState(
    @NonNull ShadowListView view,
    @Nullable ReactStylesDiffMap props,
    @Nullable StateWrapper stateWrapper) {
    view.updateState(stateWrapper);
    return super.updateState(view, props, stateWrapper);
  }
}
