package com.shadowlist;

import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.annotation.Nullable;
import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import com.facebook.react.bridge.ReadableArray;

import java.util.HashMap;

/*
 * The list tells accessibility services how many rows it holds, not only the mounted ones,
 * and offers page scrolls and scrolling to any row, like the native kit's list.
 */
class ShadowListAccessibility extends AccessibilityDelegateCompat {
  private final ShadowListView mView;

  /*
   * Every row's key from props. The index map is built when first needed.
   */
  @Nullable private ReadableArray mItemKeys = null;
  @Nullable private HashMap<String, Integer> mItemIndices = null;

  ShadowListAccessibility(ShadowListView view) {
    mView = view;
  }

  void setItemKeys(@Nullable ReadableArray keys) {
    mItemKeys = keys;
    mItemIndices = null;
  }

  @Override
  public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfoCompat info) {
    super.onInitializeAccessibilityNodeInfo(host, info);
    int columns = Math.max(1, mView.getNumberOfColumns());
    int itemCount = getItemCount();
    int lines = (itemCount + columns - 1) / columns;
    info.setClassName(columns > 1 ? "android.widget.GridView" : "android.widget.ListView");
    info.setScrollable(scrollRangePx() > 0 && mView.isScrollEnabled());
    info.setCollectionInfo(mView.isHorizontal()
      ? AccessibilityNodeInfoCompat.CollectionInfoCompat.obtain(columns, lines, false,
          AccessibilityNodeInfoCompat.CollectionInfoCompat.SELECTION_MODE_NONE)
      : AccessibilityNodeInfoCompat.CollectionInfoCompat.obtain(lines, columns, false,
          AccessibilityNodeInfoCompat.CollectionInfoCompat.SELECTION_MODE_NONE));
    info.addAction(AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_TO_POSITION);
  }

  @Override
  public void onInitializeAccessibilityEvent(View host, AccessibilityEvent event) {
    super.onInitializeAccessibilityEvent(host, event);
    event.setItemCount(getItemCount());
    int[] visible = visibleItemRange();
    if (visible != null) {
      event.setFromIndex(visible[0]);
      event.setToIndex(visible[1]);
    }
  }

  @Override
  public boolean performAccessibilityAction(View host, int action, @Nullable Bundle arguments) {
    if (!mView.isScrollEnabled()) {
      return super.performAccessibilityAction(host, action, arguments);
    }
    boolean horizontal = mView.isHorizontal();
    int forward = horizontal ? android.R.id.accessibilityActionScrollRight : android.R.id.accessibilityActionScrollDown;
    int backward = horizontal ? android.R.id.accessibilityActionScrollLeft : android.R.id.accessibilityActionScrollUp;
    if (action == AccessibilityNodeInfo.ACTION_SCROLL_FORWARD || action == forward) {
      return scrollByPage(1);
    }
    if (action == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD || action == backward) {
      return scrollByPage(-1);
    }
    if (action == android.R.id.accessibilityActionScrollToPosition) {
      return scrollToPosition(arguments);
    }
    return super.performAccessibilityAction(host, action, arguments);
  }

  private int getItemCount() {
    return mItemKeys != null ? mItemKeys.size() : 0;
  }

  private int scrollRangePx() {
    ViewGroup content = mView.getContentView();
    ViewGroup scrollView = mView.getScrollView();
    return mView.isHorizontal()
      ? Math.max(0, content.getWidth() - scrollView.getWidth())
      : Math.max(0, content.getHeight() - scrollView.getHeight());
  }

  /*
   * One viewport toward the end, or toward the start for a negative direction.
   */
  private boolean scrollByPage(int direction) {
    boolean horizontal = mView.isHorizontal();
    ViewGroup scrollView = mView.getScrollView();
    int offset = horizontal ? scrollView.getScrollX() : scrollView.getScrollY();
    int viewport = horizontal ? scrollView.getWidth() : scrollView.getHeight();
    int target = (int) ShadowListGeometry.pageScrollTarget(offset, viewport, scrollRangePx(), direction);
    if (target == offset) {
      return false;
    }
    mView.stopMomentum();
    scrollView.scrollTo(horizontal ? target : scrollView.getScrollX(), horizontal ? scrollView.getScrollY() : target);
    // The jump is over at once. Report the list at rest, or corrections would wait for a gesture end.
    mView.reportScrollPhase(ShadowListView.SCROLL_PHASE_IDLE);
    scrollView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED);
    return true;
  }

  private boolean scrollToPosition(@Nullable Bundle arguments) {
    if (arguments == null || mView.getStateWrapper() == null) {
      return false;
    }
    int row = arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_ROW_INT, -1);
    int column = arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_COLUMN_INT, 0);
    int columns = Math.max(1, mView.getNumberOfColumns());
    int index = mView.isHorizontal() ? column * columns + row : row * columns + column;
    if (index < 0 || index >= getItemCount()) {
      return false;
    }
    mView.issueScrollCommand(index, 0.0, 0.0, false);
    mView.getScrollView().sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED);
    return true;
  }

  /*
   * The lowest and highest index of the mounted rows on screen, or null when none. Rows are
   * looked up by key in the props' keys.
   */
  @Nullable
  private int[] visibleItemRange() {
    if (mItemKeys == null) {
      return null;
    }
    if (mItemIndices == null) {
      HashMap<String, Integer> indices = new HashMap<>();
      for (int index = mItemKeys.size() - 1; index >= 0; index--) {
        indices.put(mItemKeys.getString(index), index);
      }
      mItemIndices = indices;
    }
    boolean horizontal = mView.isHorizontal();
    ViewGroup content = mView.getContentView();
    ViewGroup scrollView = mView.getScrollView();
    int low = horizontal ? scrollView.getScrollX() : scrollView.getScrollY();
    int high = low + (horizontal ? scrollView.getWidth() : scrollView.getHeight());
    int first = -1;
    int last = -1;
    for (int child = 0; child < content.getChildCount(); child++) {
      View view = content.getChildAt(child);
      if (!(view instanceof ShadowListElementView)) {
        continue;
      }
      int start = horizontal ? view.getLeft() : view.getTop();
      int end = horizontal ? view.getRight() : view.getBottom();
      if (end <= low || start >= high) {
        continue;
      }
      Integer index = mItemIndices.get(((ShadowListElementView) view).getElementKey());
      if (index == null) {
        continue;
      }
      first = first < 0 ? index : Math.min(first, index);
      last = Math.max(last, index);
    }
    return first < 0 ? null : new int[] {first, last};
  }
}
