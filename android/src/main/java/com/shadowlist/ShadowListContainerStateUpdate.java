package com.shadowlist;

import androidx.annotation.Nullable;
import android.content.Context;
import android.graphics.Canvas;
import android.view.View;
import android.widget.ScrollView;

import com.facebook.react.views.view.ReactViewGroup;
import com.facebook.react.uimanager.StateWrapper;

public class ShadowListContainerStateUpdate {

  private final float mScrollPositionX;
  private final float mScrollPositionY;
  private final float mScrollContainerWidth;
  private final float mScrollContainerHeight;
  private final float mScrollContentWidth;
  private final float mScrollContentHeight;

  public ShadowListContainerStateUpdate(
    float mScrollPositionX,
    float mScrollPositionY,
    float mScrollContainerWidth,
    float mScrollContainerHeight,
    float mScrollContentWidth,
    float mScrollContentHeight) {
    this.mScrollPositionX = mScrollPositionX;
    this.mScrollPositionY = mScrollPositionY;
    this.mScrollContainerWidth = mScrollContainerWidth;
    this.mScrollContainerHeight = mScrollContainerHeight;
    this.mScrollContentWidth = mScrollContentWidth;
    this.mScrollContentHeight = mScrollContentHeight;
  }

  public float getScrollPositionX() {
    return mScrollPositionX;
  }

  public float getScrollPositionY() {
    return mScrollPositionY;
  }

  public float getScrollContainerWidth() {
    return mScrollContainerWidth;
  }

  public float getScrollContainerHeight() {
    return mScrollContainerHeight;
  }

  public float getScrollContentWidth() {
    return mScrollContentWidth;
  }

  public float getScrollContentHeight() {
    return mScrollContentHeight;
  }
}
