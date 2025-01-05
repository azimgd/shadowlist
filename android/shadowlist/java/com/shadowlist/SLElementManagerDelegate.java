package com.shadowlist;

import android.view.View;
import androidx.annotation.Nullable;
import com.facebook.react.uimanager.BaseViewManagerDelegate;
import com.facebook.react.uimanager.BaseViewManagerInterface;

public class SLElementManagerDelegate<T extends View, U extends BaseViewManagerInterface<T> & SLElementManagerInterface<T>> extends BaseViewManagerDelegate<T, U> {
  public SLElementManagerDelegate(U viewManager) {
    super(viewManager);
  }

  @Override
  public void setProperty(T view, String propName, @Nullable Object value) {
    switch (propName) {
      case "uniqueId":
        mViewManager.setUniqueId(view, value == null ? null : (String) value);
        break;
      default:
        super.setProperty(view, propName, value);
    }
  }
}
