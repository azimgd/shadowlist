package com.shadowlist;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableArray;
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
  @ReactProp(name = "rowKeys")
  public void setRowKeys(ShadowListView view, @Nullable ReadableArray rowKeys) {
    // The core reads this prop. The view only counts rows for accessibility.
    view.setItemKeys(rowKeys);
  }

  @Override
  @ReactProp(name = "keysStore")
  public void setKeysStore(ShadowListView view, double keysStore) {
    view.setKeysStore((long) keysStore);
  }

  @Override
  @ReactProp(name = "keysVersion")
  public void setKeysVersion(ShadowListView view, double keysVersion) {
    view.setKeysVersion(keysVersion);
  }

  @Override
  @ReactProp(name = "sizeSpecsVersion")
  public void setSizeSpecsVersion(ShadowListView view, double sizeSpecsVersion) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "nonAnchorKeys")
  public void setNonAnchorKeys(
      ShadowListView view, @Nullable ReadableArray nonAnchorKeys) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "rowSizeSpecs")
  public void setRowSizeSpecs(
      ShadowListView view, @Nullable String rowSizeSpecs) {
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
  @ReactProp(name = "dragBoardId")
  public void setDragBoardId(ShadowListView view, @Nullable String dragBoardId) {
    view.setDragBoardId(dragBoardId);
  }

  @Override
  @ReactProp(name = "dragListId")
  public void setDragListId(ShadowListView view, @Nullable String dragListId) {
    view.setDragListId(dragListId);
  }

  @Override
  @ReactProp(name = "dragListLabel")
  public void setDragListLabel(ShadowListView view, @Nullable String dragListLabel) {
    view.setDragListLabel(dragListLabel);
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
  @ReactProp(name = "scrollIndex")
  public void setScrollIndex(ShadowListView view, int scrollIndex) {
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
        ShadowListScrollEvent.SCROLL,
        MapBuilder.of("registrationName", "onScroll"))
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
    view.setScrollEventThrottle(scrollEventThrottle);
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
  @ReactProp(name = "progressViewOffset")
  public void setProgressViewOffset(ShadowListView view, double progressViewOffset) {
    view.setProgressViewOffset(progressViewOffset);
  }

  @Override
  @ReactProp(name = "overscan")
  public void setOverscan(ShadowListView view, double overscan) {
    // Only the core reads this prop.
  }

  @Override
  @ReactProp(name = "scrollEventEnabled")
  public void setScrollEventEnabled(ShadowListView view, boolean scrollEventEnabled) {
    view.setScrollEventEnabled(scrollEventEnabled);
  }

  @Override
  @ReactProp(name = "viewableEventEnabled")
  public void setViewableEventEnabled(ShadowListView view, boolean viewableEventEnabled) {
    // The component descriptor reads this to decide whether to watch which rows are viewable.
  }

  @Override
  @ReactProp(name = "viewableTimings")
  public void setViewableTimings(ShadowListView view, @Nullable ReadableArray viewableTimings) {
    // Only the mount driver reads these props, in the component descriptor.
  }

  @Override
  @ReactProp(name = "viewabilityInteracted")
  public void setViewabilityInteracted(ShadowListView view, boolean viewabilityInteracted) {}

  @Override
  @ReactProp(name = "visibleRangeEventEnabled")
  public void setVisibleRangeEventEnabled(ShadowListView view, boolean visibleRangeEventEnabled) {}

  @Override
  @ReactProp(name = "mountedLow", defaultInt = -1)
  public void setMountedLow(ShadowListView view, int mountedLow) {}

  @Override
  @ReactProp(name = "mountedHigh", defaultInt = -1)
  public void setMountedHigh(ShadowListView view, int mountedHigh) {}

  @Override
  @ReactProp(name = "mountedSequence")
  public void setMountedSequence(ShadowListView view, double mountedSequence) {}

  @Override
  @ReactProp(name = "mountedGeneration")
  public void setMountedGeneration(ShadowListView view, double mountedGeneration) {}

  @Override
  @ReactProp(name = "seedPending")
  public void setSeedPending(ShadowListView view, boolean seedPending) {}

  @Override
  @ReactProp(name = "mountOverscanRows", defaultInt = 4)
  public void setMountOverscanRows(ShadowListView view, int mountOverscanRows) {}

  @Override
  @ReactProp(name = "mountOverscanRowsLeading", defaultInt = 10)
  public void setMountOverscanRowsLeading(ShadowListView view, int mountOverscanRowsLeading) {}

  @Override
  @ReactProp(name = "prefetchRows")
  public void setPrefetchRows(ShadowListView view, int prefetchRows) {}

  /*
   * A commit that applied a mounted range lands here with its props. Its next step goes on the
   * next frame.
   */
  @Override
  protected void onAfterUpdateTransaction(@NonNull ShadowListView view) {
    super.onAfterUpdateTransaction(view);
    view.pokeMountTicker();
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
  public void scrollToRow(ShadowListView view, int row, double viewPosition, double viewOffset, boolean animated) {
    view.scrollToRow(row, viewPosition, viewOffset, animated);
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
