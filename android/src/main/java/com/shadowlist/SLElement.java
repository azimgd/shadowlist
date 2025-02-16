package com.shadowlist;

import android.content.Context;
import android.util.AttributeSet;
import com.facebook.react.views.view.ReactViewGroup;

public class SLElement extends ReactViewGroup {
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

  public String getUniqueId() {
    return this.mUniqueId;
  }

  public void setUniqueId(String uniqueId) {
    this.mUniqueId = uniqueId;
  }
}
