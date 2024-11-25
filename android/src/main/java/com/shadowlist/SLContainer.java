package com.shadowlist;

import android.content.Context;
import android.os.Handler;
import android.widget.HorizontalScrollView;
import android.widget.ScrollView;
import android.view.View;
import android.graphics.Canvas;
import android.util.AttributeSet;
import androidx.annotation.Nullable;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.facebook.react.common.mapbuffer.MapBuffer;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.StateWrapper;
import com.facebook.react.views.view.ReactViewGroup;

public class SLContainer extends ReactViewGroup {
  private boolean mOrientation;
  private SwipeRefreshLayout mScrollContainerRefreshVertical;
  private SwipeRefreshLayout mScrollContainerRefreshHorizontal;
  private HorizontalScrollView mScrollContainerHorizontal;
  private ScrollView mScrollContainerVertical;
  private ReactViewGroup mScrollContent;
  private SLScrollable mScrollable;
  private SLContainerChildrenManager mContainerChildrenManager;
  private SLFenwickTree mChildrenMeasurements;
  private SLContainerManager.OnStartReachedHandler mOnStartReachedHandler;
  private SLContainerManager.OnEndReachedHandler mOnEndReachedHandler;
  private SLContainerManager.OnVisibleChangeHandler mOnVisibleChangeHandler;

  private @Nullable StateWrapper mStateWrapper = null;

  public SLContainer(Context context) {
    super(context);
    init(context);
  }

  public SLContainer(Context context, AttributeSet attrs) {
    super(context);
    init(context);
  }

  private void init(Context context) {
    mScrollContainerRefreshVertical = new SwipeRefreshLayout(context);
    mScrollContainerRefreshHorizontal = new SwipeRefreshLayout(context);
    mScrollContainerHorizontal = new HorizontalScrollView(context);
    mScrollContainerVertical = new ScrollView(context);

    mScrollContent = new ReactViewGroup(context);
    mContainerChildrenManager = new SLContainerChildrenManager(mScrollContent);
    mScrollable = new SLScrollable();

    SwipeRefreshLayout.OnRefreshListener refreshListener = () -> {
    };
    OnScrollChangeListener scrollListenerVertical = (v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
      this.updateVirtualization();
    };
    OnScrollChangeListener scrollListenerHorizontal = (v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
      this.updateVirtualization();
    };
    mScrollContainerVertical.setOnScrollChangeListener(scrollListenerVertical);
    mScrollContainerVertical.setVerticalScrollBarEnabled(true);
    mScrollContainerHorizontal.setOnScrollChangeListener(scrollListenerHorizontal);
    mScrollContainerHorizontal.setHorizontalScrollBarEnabled(true);
    mScrollContainerRefreshVertical.setOnRefreshListener(refreshListener);
    mScrollContainerRefreshHorizontal.setOnRefreshListener(refreshListener);
  }

  public void updateVirtualization() {
    if (mStateWrapper == null) return;

    MapBuffer stateMapBuffer = mStateWrapper.getStateDataMapBuffer();

    float[] scrollPosition = new float[]{
      PixelUtil.toDIPFromPixel(this.mScrollContainerVertical.getScrollX()),
      PixelUtil.toDIPFromPixel(this.mScrollContainerVertical.getScrollY())};

    int visibleStartIndex = mChildrenMeasurements.adjustVisibleStartIndex(
      mChildrenMeasurements.lowerBound(mScrollable.getVisibleStartOffset(scrollPosition)),
      stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE)
    );
    int visibleEndIndex = mChildrenMeasurements.adjustVisibleEndIndex(
      mChildrenMeasurements.lowerBound(mScrollable.getVisibleEndOffset(scrollPosition)),
      stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE)
    );
    mContainerChildrenManager.mount(
      visibleStartIndex,
      visibleEndIndex
    );

    updateObservers(scrollPosition, visibleStartIndex, visibleEndIndex);
  }

  public void updateObservers(float[] scrollPosition, int visibleStartIndex, int visibleEndIndex) {
    mOnVisibleChangeHandler.onVisibleChange(this, visibleStartIndex, visibleEndIndex);

    int distanceFromStart = mScrollable.shouldNotifyStart(scrollPosition);
    if (distanceFromStart > 0) {
      mOnStartReachedHandler.onStartReached(this, distanceFromStart);
    }

    int distanceFromEnd = mScrollable.shouldNotifyEnd(scrollPosition);
    if (distanceFromEnd > 0) {
      mOnEndReachedHandler.onEndReached(this, distanceFromEnd);
    }
  }

  public void setScrollContainerHorizontal() {
    if (mOrientation) return;
    mOrientation = true;
    mScrollContainerHorizontal.addView(mScrollContent, 0);
    mScrollContainerRefreshHorizontal.addView(mScrollContainerHorizontal, 0);
    super.addView(mScrollContainerRefreshHorizontal, 0);
  }

  public void setScrollContainerVertical() {
    if (mOrientation) return;
    mOrientation = true;
    mScrollContainerVertical.addView(mScrollContent, 0);
    mScrollContainerRefreshVertical.addView(mScrollContainerVertical, 0);
    super.addView(mScrollContainerRefreshVertical, 0);
  }

  public void setScrollContentLayout(float width, float height) {
    mScrollContent.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerLayout(float width, float height) {
    mScrollContainerRefreshHorizontal.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
    mScrollContainerHorizontal.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));

    mScrollContainerRefreshVertical.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
    mScrollContainerVertical.layout(0, 0, (int) PixelUtil.toPixelFromDIP(width), (int) PixelUtil.toPixelFromDIP(height));
  }

  public void setScrollContainerOffset(int x, int y) {
  }

  @Override
  protected void onLayout(boolean changed, int l, int t, int r, int b) {
    if (!mOrientation) {
      setScrollContainerVertical();
    }
  }

  @Override
  public void addView(View child, int index) {
    mContainerChildrenManager.mountChildComponentView(child, ((SLElement)child).getUniqueId());
  }

  @Override
  public void removeView(View child) {
    mContainerChildrenManager.mountChildComponentView(child, ((SLElement)child).getUniqueId());
  }

  @Override
  protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec));
  }

  @Override
  public void draw(Canvas canvas) {
    super.draw(canvas);
  }

  public void setStateWrapper(
    StateWrapper stateWrapper,
    SLContainerManager.OnStartReachedHandler onStartReachedHandler,
    SLContainerManager.OnEndReachedHandler onEndReachedHandler,
    SLContainerManager.OnVisibleChangeHandler onVisibleChangeHandler) {
    MapBuffer stateMapBuffer = stateWrapper.getStateDataMapBuffer();

    float[] childrenMeasurements = new float[stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE)];
    for (int i = 0; i < stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE); i++) {
      childrenMeasurements[i] = (float) stateMapBuffer.getMapBuffer(SLContainerManager.SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE).getDouble(i);
    }

    mChildrenMeasurements = new SLFenwickTree(childrenMeasurements);

    mScrollable.updateState(
      stateMapBuffer.getBoolean(SLContainerManager.SLCONTAINER_STATE_HORIZONTAL),
      false,
      (float) stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH),
      (float) stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT),
      (float) stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH),
      (float) stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT)
    );

    float[] scrollPosition = new float[]{
      (float) stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_POSITION_LEFT) + PixelUtil.toDIPFromPixel(mScrollContainerVertical.getScrollX()),
      (float) stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_POSITION_TOP) + PixelUtil.toDIPFromPixel(mScrollContainerVertical.getScrollY())};

    this.setScrollContainerLayout(
      (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTAINER_WIDTH),
      (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTAINER_HEIGHT)
    );

    this.setScrollContentLayout(
      (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTENT_WIDTH),
      (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_CONTENT_HEIGHT)
    );

    this.setScrollContainerOffset(
      (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_POSITION_LEFT),
      (int)stateMapBuffer.getDouble(SLContainerManager.SLCONTAINER_STATE_SCROLL_POSITION_TOP)
    );

    int visibleStartIndex = mChildrenMeasurements.adjustVisibleStartIndex(
      mChildrenMeasurements.lowerBound(mScrollable.getVisibleStartOffset(scrollPosition)),
      stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE)
    );
    int visibleEndIndex = mChildrenMeasurements.adjustVisibleEndIndex(
      mChildrenMeasurements.lowerBound(mScrollable.getVisibleEndOffset(scrollPosition)),
      stateMapBuffer.getInt(SLContainerManager.SLCONTAINER_STATE_CHILDREN_MEASUREMENTS_TREE_SIZE)
    );

    mContainerChildrenManager.mount(
      visibleStartIndex,
      visibleEndIndex
    );

    mOnStartReachedHandler = onStartReachedHandler;
    mOnEndReachedHandler = onEndReachedHandler;
    mOnVisibleChangeHandler = onVisibleChangeHandler;
    mStateWrapper = stateWrapper;

    mScrollContainerVertical.scrollTo((int)PixelUtil.toPixelFromDIP(scrollPosition[0]), (int)PixelUtil.toPixelFromDIP(scrollPosition[1]));

    Handler handler = new Handler();
    handler.postDelayed(new Runnable() {
      @Override
      public void run() {
        updateVirtualization();
      }
    }, 16);
  }
}
