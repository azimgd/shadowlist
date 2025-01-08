package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
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
