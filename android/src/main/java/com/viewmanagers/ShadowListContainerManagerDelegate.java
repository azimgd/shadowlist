package com.shadowlist;

import android.view.View;
import androidx.annotation.Nullable;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.uimanager.BaseViewManagerDelegate;
import com.facebook.react.uimanager.BaseViewManagerInterface;

public class ShadowListContainerManagerDelegate<T extends View, U extends BaseViewManagerInterface<T> & ShadowListContainerManagerInterface<T>> extends BaseViewManagerDelegate<T, U> {
  public ShadowListContainerManagerDelegate(U viewManager) {
    super(viewManager);
  }

  @Override
  public void setProperty(T view, String propName, @Nullable Object value) {
    switch (propName) {
      case "inverted":
        mViewManager.setInverted(view, value == null ? false : (boolean) value);
        break;
      case "horizontal":
        mViewManager.setHorizontal(view, value == null ? false : (boolean) value);
        break;

      default:
        super.setProperty(view, propName, value);
    }
  }

  @Override
  public void receiveCommand(T view, String commandName, ReadableArray args) {
    switch (commandName) {
      case "scrollToIndex":
        mViewManager.scrollToIndex(view, args.getInt(0));
        break;
      case "scrollToOffset":
        mViewManager.scrollToOffset(view, args.getInt(0));
        break;
    }
  }
}
