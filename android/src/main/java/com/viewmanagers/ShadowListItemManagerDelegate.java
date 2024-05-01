package com.shadowlist;

import android.view.View;
import androidx.annotation.Nullable;
import com.facebook.react.uimanager.BaseViewManagerDelegate;
import com.facebook.react.uimanager.BaseViewManagerInterface;

public class ShadowListItemManagerDelegate<T extends View, U extends BaseViewManagerInterface<T> & ShadowListItemManagerInterface<T>> extends BaseViewManagerDelegate<T, U> {
  public ShadowListItemManagerDelegate(U viewManager) {
    super(viewManager);
  }

  @Override
  public void setProperty(T view, String propName, @Nullable Object value) {
    super.setProperty(view, propName, value);
  }
}
