package com.shadowlist;

import android.view.View;
import androidx.annotation.Nullable;
import com.facebook.react.uimanager.BaseViewManagerDelegate;
import com.facebook.react.uimanager.BaseViewManagerInterface;

public class SLContentManagerDelegate<T extends View, U extends BaseViewManagerInterface<T> & SLContentManagerInterface<T>> extends BaseViewManagerDelegate<T, U> {
  public SLContentManagerDelegate(U viewManager) {
    super(viewManager);
  }

  @Override
  public void setProperty(T view, String propName, @Nullable Object value) {
    switch (propName) {
      default:
        super.setProperty(view, propName, value);
    }
  }
}
