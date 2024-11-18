package com.shadowlist;

import android.view.View;
import com.facebook.react.views.view.ReactViewGroup;

import java.util.HashMap;
import java.util.Map;

public class SLContainerChildrenManager {
  private ReactViewGroup mScrollContent;
  private SLComponentRegistry mChildrenRegistry;
  private Map<String, View> mChildrenPool;

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

  public void mountChildComponentView(View childComponentView, String uniqueId) {
    mChildrenPool.put(uniqueId, childComponentView);
    mChildrenRegistry.registerComponent(uniqueId);
  }

  public void unmountChildComponentView(View childComponentView, String uniqueId) {
    mChildrenRegistry.unregisterComponent(uniqueId);
    mChildrenPool.remove(uniqueId);
  }

  public void mount(int visibleStartIndex, int visibleEndIndex) {
    String[] mounted = new String[mChildrenPool.size()];
    int index = 0;

    for (Map.Entry<String, View> entry : mChildrenPool.entrySet()) {
      SLElement childComponentView = (SLElement)entry.getValue();

      if (childComponentView.getIndex() >= visibleStartIndex && childComponentView.getIndex() <= visibleEndIndex) {
        mounted[index++] = childComponentView.getUniqueId();
      }
    }

    mChildrenRegistry.mount(mounted);
  }

  public void destroy() {
    for (String uniqueId : mChildrenPool.keySet()) {
      unmountChildComponentView(mChildrenPool.get(uniqueId), uniqueId);
    }
    mChildrenRegistry.destroy();
  }
}
