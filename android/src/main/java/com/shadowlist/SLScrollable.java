package com.shadowlist;

public class SLScrollable {
  private boolean mHorizontal;
  private boolean mInverted;
  private float mScrollContainerWidth;
  private float mScrollContainerHeight;
  private float mScrollContentWidth;
  private float mScrollContentHeight;
  private float mVisibleStartTrigger;
  private float mVisibleEndTrigger;
  private float[] mScrollContentOffset;
  private float mLastContentOffsetX;
  private float mLastContentOffsetY;
  private float mStartContentOffsetX;
  private float mStartContentOffsetY;
  private long mLastUpdateTime;

  private static final int SCROLLING_UP = 1;
  private static final int SCROLLING_DOWN = 2;
  private static final int SCROLLING_LEFT = 3;
  private static final int SCROLLING_RIGHT = 4;

  public SLScrollable() {
    this.mScrollContentOffset = new float[2];
    this.mLastUpdateTime = System.nanoTime();
  }

  public void updateState(boolean horizontal,
    boolean inverted,
    float visibleStartTrigger,
    float visibleEndTrigger,
    float scrollContainerWidth,
    float scrollContainerHeight,
    float scrollContentWidth,
    float scrollContentHeight) {
    this.mHorizontal = horizontal;
    this.mInverted = inverted;
    this.mVisibleStartTrigger = visibleStartTrigger;
    this.mVisibleEndTrigger = visibleEndTrigger;
    this.mScrollContainerWidth = scrollContainerWidth;
    this.mScrollContainerHeight = scrollContainerHeight;
    this.mScrollContentWidth = scrollContentWidth;
    this.mScrollContentHeight = scrollContentHeight;
  }

  public boolean shouldUpdate(float[] contentOffset) {
    this.mLastContentOffsetX = contentOffset[0];
    this.mLastContentOffsetY = contentOffset[1];

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

  public int checkNotifyStart(float[] contentOffset) {
    if (mHorizontal && contentOffset[0] < mScrollContainerWidth) {
      return (int) contentOffset[0];
    } else if (!mHorizontal && contentOffset[1] < mScrollContainerHeight) {
      return (int) contentOffset[1];
    }
    return 0;
  }

  public int checkNotifyEnd(float[] contentOffset) {
    if (mHorizontal && contentOffset[0] > mScrollContentWidth - mScrollContainerWidth) {
      return Math.abs((int) (mScrollContentWidth - contentOffset[0]));
    } else if (!mHorizontal && contentOffset[1] > mScrollContentHeight - mScrollContainerHeight) {
      return Math.abs((int) (mScrollContentHeight - contentOffset[1]));
    }
    return 0;
  }

  public int shouldNotifyStart(float[] contentOffset) {
    if (!mInverted) {
      return checkNotifyStart(contentOffset);
    } else {
      return checkNotifyEnd(contentOffset);
    }
  }

  public int shouldNotifyEnd(float[] contentOffset) {
    if (!mInverted) {
      return checkNotifyEnd(contentOffset);
    } else {
      return checkNotifyStart(contentOffset);
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

  public float[] calculateVelocity() {
    long currentTime = System.nanoTime();
    float timeSinceLastUpdate = (currentTime - mLastUpdateTime) / 1_000_000_000.0f;

    if (timeSinceLastUpdate > 0.01f) {
      float velocityX = (mLastContentOffsetX - mStartContentOffsetX) / timeSinceLastUpdate;
      float velocityY = (mLastContentOffsetY - mStartContentOffsetY) / timeSinceLastUpdate;

      mStartContentOffsetX = mLastContentOffsetX;
      mStartContentOffsetY = mLastContentOffsetY;

      mLastUpdateTime = currentTime;

      return new float[]{velocityX, velocityY};
    }

    return new float[]{0.0f, 0.0f};
  }
}
