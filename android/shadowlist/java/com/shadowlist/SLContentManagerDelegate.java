package com.shadowlist;

import android.view.View;
import androidx.annotation.Nullable;
import com.facebook.react.uimanager.BaseViewManagerDelegate;
import com.facebook.react.uimanager.BaseViewManager;
import com.facebook.react.uimanager.LayoutShadowNode;

public class SLContentManagerDelegate<T extends View, U extends BaseViewManager<T, ? extends LayoutShadowNode> & SLContentManagerInterface<T>> extends BaseViewManagerDelegate<T, U> {
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
