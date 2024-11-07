package com.shadowlist;

import android.view.View;
import com.facebook.react.views.view.ReactViewGroup;
import java.util.HashMap;
import java.util.Map;

public class SLContainerChildrenManager {
  private ReactViewGroup mScrollContent;
  private SLComponentRegistry mChildrenRegistry;
  private Map<Integer, View> mChildrenPool;

  public SLContainerChildrenManager(ReactViewGroup contentView) {
    this.mScrollContent = contentView;
    this.mChildrenRegistry = new SLComponentRegistry();

    mChildrenRegistry.mountObserver((index, isVisible) -> {
      try {
        mountObserver(index, isVisible);
      } catch (IndexOutOfBoundsException e) {}
    });

    this.mChildrenPool = new HashMap<>();
  }

  private void mountObserver(int index, boolean isVisible) {
    View child = mChildrenPool.get(index);

    if (isVisible) {
      mScrollContent.addView(child);
    } else {
      mScrollContent.removeView(child);
    }
  }

  public void mountChildComponentView(View childComponentView, int index) {
    mChildrenPool.put(index, childComponentView);
    mChildrenRegistry.registerComponent(index);
  }

  public void unmountChildComponentView(View childComponentView, int index) {
    mChildrenRegistry.unregisterComponent(index);
    mChildrenPool.remove(index);
  }

  public void mount(int visibleStartIndex, int visibleEndIndex) {
    mChildrenRegistry.mountRange(visibleStartIndex, visibleEndIndex);
  }

  public void destroy() {
    for (Integer index : mChildrenPool.keySet()) {
      unmountChildComponentView(mChildrenPool.get(index), index);
    }
    mChildrenRegistry.destroy();
  }
}
