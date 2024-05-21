package com.shadowlist;

import android.view.View;

public interface ShadowListContainerManagerInterface<T extends View> {
  void setInverted(T view, boolean value);
  void setHorizontal(T view, boolean value);
  void scrollToIndex(T view, int index, boolean animated);
  void scrollToOffset(T view, int offset, boolean animated);
}
