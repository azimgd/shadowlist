package com.shadowlist;

import android.view.View;
import androidx.annotation.Nullable;

public interface SLElementManagerInterface<T extends View> {
  void setUniqueId(T view, @Nullable String value);
}
