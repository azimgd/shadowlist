package com.shadowlist;

public class SLScrollable {
  private boolean mHorizontal;
  private float mScrollContainerWidth;
  private float mScrollContainerHeight;
  private float mVisibleStartTrigger;
  private float mVisibleEndTrigger;
  private float[] mScrollContentOffset;

  private static final int SCROLLING_UP = 1;
  private static final int SCROLLING_DOWN = 2;
  private static final int SCROLLING_LEFT = 3;
  private static final int SCROLLING_RIGHT = 4;

  public SLScrollable() {
    this.mScrollContentOffset = new float[2];
  }

  public void updateState(boolean horizontal,
    float visibleStartTrigger,
    float visibleEndTrigger,
    float scrollContainerWidth,
    float scrollContainerHeight) {
    this.mHorizontal = horizontal;
    this.mVisibleStartTrigger = visibleStartTrigger;
    this.mVisibleEndTrigger = visibleEndTrigger;
    this.mScrollContainerWidth = scrollContainerWidth;
    this.mScrollContainerHeight = scrollContainerHeight;
  }

  public boolean shouldUpdate(float[] contentOffset) {
    if (contentOffset[0] < 0 || contentOffset[1] < 0) {
      return true;
    }

    if (mHorizontal) {
      if (scrollDirectionHorizontal(contentOffset) == SCROLLING_LEFT) {
        return mVisibleEndTrigger >= (contentOffset[0] + mScrollContainerWidth);
      } else {
        return mVisibleStartTrigger <= contentOffset[0];
      }
    } else {
      if (scrollDirectionVertical(contentOffset) == SCROLLING_DOWN) {
        return mVisibleEndTrigger >= (contentOffset[1] + mScrollContainerHeight);
      } else {
        return mVisibleStartTrigger <= contentOffset[1];
      }
    }
  }

  private int scrollDirectionVertical(float[] contentOffset) {
    int scrollDirection;
    if (contentOffset[1] > mScrollContentOffset[1]) {
      scrollDirection = SCROLLING_DOWN;
    } else {
      scrollDirection = SCROLLING_UP;
    }

    mScrollContentOffset[1] = contentOffset[1];
    return scrollDirection;
  }

  private int scrollDirectionHorizontal(float[] contentOffset) {
    int scrollDirection;
    if (contentOffset[0] > mScrollContentOffset[0]) {
      scrollDirection = SCROLLING_RIGHT;
    } else {
      scrollDirection = SCROLLING_LEFT;
    }

    mScrollContentOffset[0] = contentOffset[0];
    return scrollDirection;
  }
}
