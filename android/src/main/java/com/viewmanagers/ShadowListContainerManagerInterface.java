package com.shadowlist;

import android.view.View;

public interface ShadowListContainerManagerInterface<T extends View> {
  void setInverted(T view, boolean value);
  void scrollToIndex(T view, int index);
  void scrollToOffset(T view, int offset);
}
