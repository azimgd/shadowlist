package com.shadowlist;

import android.content.Context;
import android.widget.HorizontalScrollView;
import android.widget.ScrollView;
import android.view.View;
import android.graphics.Canvas;
import android.util.AttributeSet;
import androidx.annotation.Nullable;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.common.mapbuffer.MapBuffer;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.view.ReactViewGroup;

public class SLElement extends ReactViewGroup {
  private int mIndex;
  private String mUniqueId;

  public SLElement(Context context) {
    super(context);
    init(context);
  }

  public SLElement(Context context, AttributeSet attrs) {
    super(context);
    init(context);
  }

  private void init(Context context) {
  }

  public int getIndex() {
    return this.mIndex;
  }

  public String getUniqueId() {
    return this.mUniqueId;
  }

  public void setIndex(int index) {
    this.mIndex = index;
  }

  public void setUniqueId(String uniqueId) {
    this.mUniqueId = uniqueId;
  }
}
