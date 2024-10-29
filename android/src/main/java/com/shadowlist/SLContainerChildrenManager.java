package com.shadowlist;

import android.view.View;
import android.widget.LinearLayout;

import java.util.HashMap;
import java.util.Map;

public class SLContainerChildrenManager {
  private LinearLayout contentView;
  private SLComponentRegistry childrenRegistry;
  private Map<Integer, View> childrenPool;

  public SLContainerChildrenManager(LinearLayout contentView) {
    this.contentView = contentView;
    this.childrenRegistry = new SLComponentRegistry(10);

    childrenRegistry.mountObserver((index, isVisible) -> {
      try {
        mountObserver(index, isVisible);
      } catch (IndexOutOfBoundsException e) {}
    });

    this.childrenPool = new HashMap<>();
  }

  private void mountObserver(int index, boolean isVisible) {
    View child = childrenPool.get(index);
    
    if (isVisible) {
      contentView.addView(child);
    } else {
      contentView.removeView(child);
    }
  }

  public void mountChildComponentView(View childComponentView, int index) {
    childrenPool.put(index, childComponentView);
    childrenRegistry.registerComponent(index);
  }

  public void unmountChildComponentView(View childComponentView, int index) {
    childrenRegistry.unregisterComponent(index);
    childrenPool.remove(index);
  }

  public void mount(int visibleStartIndex, int visibleEndIndex) {
    childrenRegistry.mountRange(visibleStartIndex, visibleEndIndex);
  }

  public void destroy() {
    for (Integer index : childrenPool.keySet()) {
      unmountChildComponentView(childrenPool.get(index), index);
    }
    childrenRegistry.destroy();
  }
}
