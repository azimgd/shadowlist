package com.shadowlist;

import android.view.View;
import androidx.annotation.Nullable;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.uimanager.BaseViewManagerDelegate;
import com.facebook.react.uimanager.BaseViewManagerInterface;

public class SLContainerManagerDelegate<T extends View, U extends BaseViewManagerInterface<T> & SLContainerManagerInterface<T>> extends BaseViewManagerDelegate<T, U> {
  public SLContainerManagerDelegate(U viewManager) {
    super(viewManager);
  }
  @Override
  public void setProperty(T view, String propName, @Nullable Object value) {
    switch (propName) {
      case "data":
        mViewManager.setData(view, value == null ? null : (String) value);
        break;
      case "inverted":
        mViewManager.setInverted(view, value == null ? false : (boolean) value);
        break;
      case "horizontal":
        mViewManager.setHorizontal(view, value == null ? false : (boolean) value);
        break;
      case "initialNumToRender":
        mViewManager.setInitialNumToRender(view, value == null ? 0 : ((Double) value).intValue());
        break;
      case "windowSize":
        mViewManager.setWindowSize(view, value == null ? 0 : ((Double) value).intValue());
        break;
      case "numColumns":
        mViewManager.setNumColumns(view, value == null ? 0 : ((Double) value).intValue());
        break;
      case "initialScrollIndex":
        mViewManager.setInitialScrollIndex(view, value == null ? 0 : ((Double) value).intValue());
        break;
      default:
        super.setProperty(view, propName, value);
    }
  }

  @Override
  public void receiveCommand(T view, String commandName, @Nullable ReadableArray args) {
    switch (commandName) {
      case "scrollToIndex":
        mViewManager.scrollToIndex(view, args.getInt(0), args.getBoolean(1));
        break;
      case "scrollToOffset":
        mViewManager.scrollToOffset(view, args.getInt(0), args.getBoolean(1));
        break;
    }
  }
}
