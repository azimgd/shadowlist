package com.shadowlist;

import android.view.View;
import androidx.annotation.Nullable;

public interface SLContainerManagerInterface<T extends View> {
  void setData(T view, @Nullable String value);
  void setInverted(T view, boolean value);
  void setHorizontal(T view, boolean value);
  void setInitialNumToRender(T view, int value);
  void setInitialScrollIndex(T view, int value);
  void scrollToIndex(T view, int index, boolean animated);
  void scrollToOffset(T view, int offset, boolean animated);
}
