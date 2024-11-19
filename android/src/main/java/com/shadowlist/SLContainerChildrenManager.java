package com.shadowlist;

import android.view.View;
import com.facebook.react.views.view.ReactViewGroup;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

public class SLContainerChildrenManager {
  private ReactViewGroup mScrollContent;
  private SLComponentRegistry mChildrenRegistry;
  private Map<String, View> mChildrenPool;

  public SLContainerChildrenManager(ReactViewGroup contentView) {
    this.mScrollContent = contentView;
    this.mChildrenRegistry = new SLComponentRegistry();

    mChildrenRegistry.mountObserver((uniqueId, isVisible) -> {
      try {
        mountObserver(uniqueId, isVisible);
      } catch (IndexOutOfBoundsException e) {}
    });

    this.mChildrenPool = new HashMap<>();
  }

  private void mountObserver(String uniqueId, boolean isVisible) {
    View child = mChildrenPool.get(uniqueId);

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
    List<String> mounted = new ArrayList<>();

    for (Map.Entry<String, View> entry : mChildrenPool.entrySet()) {
      SLElement childComponentView = (SLElement) entry.getValue();

      if (childComponentView.getIndex() >= visibleStartIndex && childComponentView.getIndex() <= visibleEndIndex) {
        mounted.add(childComponentView.getUniqueId());
      }
    }

    String[] mountedArray = mounted.toArray(new String[0]);
    mChildrenRegistry.mount(mountedArray);
  }


  public void destroy() {
    for (String uniqueId : mChildrenPool.keySet()) {
      unmountChildComponentView(mChildrenPool.get(uniqueId), uniqueId);
    }
    mChildrenRegistry.destroy();
  }
}
