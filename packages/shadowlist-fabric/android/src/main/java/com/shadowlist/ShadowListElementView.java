package com.shadowlist;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.HapticFeedbackConstants;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.view.ViewParent;
import android.view.animation.DecelerateInterpolator;
import android.widget.PopupMenu;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.events.EventDispatcher;
import com.facebook.react.uimanager.events.NativeGestureUtil;

import java.util.ArrayList;

public class ShadowListElementView extends ViewGroup {
  /*
   * Copy of the index prop. Starts at the prop default, since a props diff leaves out
   * values equal to the default and index 0 would never arrive.
   */
  private int mElementIndex = 0;

  // Copy of the elementKey prop. Drag to reorder sends it so JS can move the right item.
  private String mElementKey = "";

  public ShadowListElementView(Context context) {
    super(context);
    init(context);
  }

  public void setElementIndex(int index) {
    mElementIndex = index;
  }

  public int getElementIndex() {
    return mElementIndex;
  }

  public void setElementKey(String key) {
    mElementKey = key != null ? key : "";
  }

  public String getElementKey() {
    return mElementKey;
  }

  // region Swipe actions

  private static final float BUTTON_TEXT_SP = 15f;
  private static final int DESTRUCTIVE_COLOR = Color.rgb(255, 59, 48);

  @Nullable private ReadableArray mLeadingSwipeActions = null;
  @Nullable private ReadableArray mTrailingSwipeActions = null;
  private boolean mLeadingFullSwipe = true;
  private boolean mTrailingFullSwipe = true;

  private final ShadowListSwipeReveal mSwipe = new ShadowListSwipeReveal();
  private int mTouchSlop;
  @Nullable private VelocityTracker mVelocityTracker = null;
  @Nullable private ValueAnimator mSwipeAnimator = null;
  private float mSwipeOffset = 0f;
  private float mDownX = 0f;
  private float mDownY = 0f;
  private boolean mSwipeTracking = false;
  // The touch was taken from JS. It ends the native gesture on release.
  private boolean mSwipeTouchClaimed = false;
  // The button under an open row's touch, or -1.
  private int mButtonTouch = -1;
  // The row slid all the way out and waits for its action's result.
  private boolean mSwipedOut = false;

  /*
   * Button sizes of each side in px, measured when a swipe starts. mSpans holds the revealed
   * gap's start and size, then each shown button's start and size along the cross axis.
   */
  private double[] mLeadingSizes = new double[0];
  private double[] mTrailingSizes = new double[0];
  private double[] mSpans = new double[2];
  private int mShownCount = 0;
  private boolean mShownLeading = false;
  private final Paint mButtonPaint = new Paint();
  private final Paint mTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

  public void setLeadingSwipeActions(@Nullable ReadableArray actions) {
    boolean changed = !sameActions(mLeadingSwipeActions, actions);
    mLeadingSwipeActions = actions;
    if (changed) {
      swipeActionsChanged();
    }
  }

  public void setTrailingSwipeActions(@Nullable ReadableArray actions) {
    boolean changed = !sameActions(mTrailingSwipeActions, actions);
    mTrailingSwipeActions = actions;
    if (changed) {
      swipeActionsChanged();
    }
  }

  private static boolean sameActions(@Nullable ReadableArray previous, @Nullable ReadableArray next) {
    if (previous == next) {
      return true;
    }
    if (previous == null || next == null) {
      return false;
    }
    return previous.toArrayList().equals(next.toArrayList());
  }

  /*
   * Open buttons show the previous actions. Close without animation, unless the row is slid out
   * by a full swipe whose action changed it. That row slides back.
   */
  private void swipeActionsChanged() {
    if (isSwipeOpen()) {
      closeSwipeActions(mSwipedOut);
    }
    updateAccessibilityActions();
  }

  public void setLeadingFullSwipe(boolean fullSwipe) {
    mLeadingFullSwipe = fullSwipe;
  }

  public void setTrailingFullSwipe(boolean fullSwipe) {
    mTrailingFullSwipe = fullSwipe;
  }

  /*
   * Slide a swiped row back.
   */
  public void closeSwipeActions(boolean animated) {
    mSwipedOut = false;
    if (!isSwipeOpen()) {
      return;
    }
    if (animated && isAttachedToWindow()) {
      animateSwipe(0f, null);
      return;
    }
    cancelSwipeAnimator();
    tearDownSwipe();
  }

  /*
   * The full swipe's action finished and the row is still here. Slide it back.
   */
  public void closeFullSwipe() {
    if (mSwipedOut) {
      closeSwipeActions(true);
    }
  }

  boolean isSwipeOpen() {
    return mSwipeOffset != 0f || mSwipeAnimator != null;
  }

  // Whether the row slid all the way out and waits for its action.
  boolean isSwipedOut() {
    return mSwipedOut;
  }

  @Override
  public boolean onInterceptTouchEvent(MotionEvent event) {
    switch (event.getActionMasked()) {
      case MotionEvent.ACTION_DOWN:
        // An open row takes every touch. A tap closes it and a swipe moves it.
        return swipeDown(event);
      case MotionEvent.ACTION_MOVE:
        return swipeMove(event);
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_CANCEL:
        mSwipeTracking = false;
        mButtonTouch = -1;
        return false;
      default:
        return false;
    }
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    switch (event.getActionMasked()) {
      case MotionEvent.ACTION_DOWN:
        // No child took the touch. Keep it while a swipe may start from here.
        return swipeDown(event) || (canSwipe() && hasSwipeActions());
      case MotionEvent.ACTION_MOVE:
        swipeMove(event);
        return true;
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_CANCEL:
        swipeUp(event);
        return true;
      default:
        return true;
    }
  }

  /*
   * A touch begins. Returns whether the row is open and takes the touch.
   */
  private boolean swipeDown(MotionEvent event) {
    mSwipeTracking = false;
    mButtonTouch = -1;
    mDownX = event.getX();
    mDownY = event.getY();
    if (mVelocityTracker != null) {
      mVelocityTracker.clear();
    }
    trackVelocity(event);
    if (!isSwipeOpen()) {
      return false;
    }
    if (!mSwipedOut) {
      mButtonTouch = buttonAt(event.getX());
    }
    claimTouch(event);
    return true;
  }

  /*
   * Returns whether a swipe runs and owns the touch.
   */
  private boolean swipeMove(MotionEvent event) {
    trackVelocity(event);
    if (mSwipedOut) {
      return true;
    }
    float deltaX = event.getX() - mDownX;
    if (!mSwipeTracking) {
      float deltaY = event.getY() - mDownY;
      boolean moved = Math.abs(deltaX) > mTouchSlop || Math.abs(deltaY) > mTouchSlop;
      if (moved) {
        mButtonTouch = -1;
      }
      if (Math.abs(deltaX) <= mTouchSlop || Math.abs(deltaX) <= Math.abs(deltaY) || !beginSwipe(deltaX)) {
        return false;
      }
      mSwipeTracking = true;
      ViewParent parent = getParent();
      if (parent != null) {
        parent.requestDisallowInterceptTouchEvent(true);
      }
      claimTouch(event);
    }
    boolean wasPast = mSwipe.isPastFullSwipe(mSwipeOffset);
    applySwipeOffset((float) mSwipe.drag(deltaX));
    if (mSwipe.isPastFullSwipe(mSwipeOffset) != wasPast) {
      performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK);
    }
    return true;
  }

  private void swipeUp(MotionEvent event) {
    trackVelocity(event);
    boolean released = event.getActionMasked() == MotionEvent.ACTION_UP;
    if (mSwipeTracking) {
      mSwipeTracking = false;
      float velocity = 0f;
      if (released && mVelocityTracker != null) {
        mVelocityTracker.computeCurrentVelocity(1000);
        velocity = mVelocityTracker.getXVelocity();
      }
      settleSwipe(velocity);
    } else if (released && mButtonTouch >= 0 && buttonAt(event.getX()) == mButtonTouch) {
      performSwipeAction(mShownLeading, mButtonTouch);
    } else if (released && isSwipeOpen() && !mSwipedOut) {
      // A tap on the open row closes it.
      closeSwipeActions(true);
    }
    mButtonTouch = -1;
    if (mSwipeTouchClaimed) {
      mSwipeTouchClaimed = false;
      NativeGestureUtil.notifyNativeGestureEnded(this, event);
    }
  }

  private void trackVelocity(MotionEvent event) {
    if (mVelocityTracker == null) {
      mVelocityTracker = VelocityTracker.obtain();
    }
    mVelocityTracker.addMovement(event);
  }

  /*
   * Cancel the JS touch. A pressed row inside must not fire on release.
   */
  private void claimTouch(MotionEvent event) {
    if (!mSwipeTouchClaimed) {
      mSwipeTouchClaimed = true;
      NativeGestureUtil.notifyNativeGestureStarted(this, event);
    }
  }

  @Nullable
  private ShadowListView listView() {
    ViewParent parent = getParent();
    while (parent != null) {
      if (parent instanceof ShadowListView) {
        return (ShadowListView) parent;
      }
      parent = parent.getParent();
    }
    return null;
  }

  /*
   * Swipes run across a vertical list, never during a drag to reorder.
   */
  private boolean canSwipe() {
    ShadowListView list = listView();
    return list != null && !list.isHorizontal() && !list.isReorderDragging();
  }

  private boolean hasSwipeActions() {
    return actionCount(mLeadingSwipeActions) > 0 || actionCount(mTrailingSwipeActions) > 0;
  }

  /*
   * Start swiping toward a side that has actions, or move an open row again.
   */
  private boolean beginSwipe(float deltaX) {
    if (!canSwipe()) {
      return false;
    }
    if (!isSwipeOpen()) {
      if (actionCount(deltaX > 0 ? mLeadingSwipeActions : mTrailingSwipeActions) == 0) {
        return false;
      }
      ShadowListView list = listView();
      if (list != null) {
        list.swipeRowOpened(this);
      }
      mLeadingSizes = measureButtons(mLeadingSwipeActions);
      mTrailingSizes = measureButtons(mTrailingSwipeActions);
      setWillNotDraw(false);
    }
    cancelSwipeAnimator();
    mSwipe.begin(
      sum(mLeadingSizes),
      sum(mTrailingSizes),
      mLeadingFullSwipe && mLeadingSizes.length > 0,
      mTrailingFullSwipe && mTrailingSizes.length > 0,
      getWidth(),
      mSwipeOffset);
    return true;
  }

  private double[] measureButtons(@Nullable ReadableArray actions) {
    float density = getResources().getDisplayMetrics().density;
    double[] sizes = new double[actionCount(actions)];
    for (int index = 0; index < sizes.length; index++) {
      float text = mTextPaint.measureText(actionTitle(actions, index));
      sizes[index] = ShadowListSwipeReveal.buttonSize(text, density);
    }
    return sizes;
  }

  private static double sum(double[] values) {
    double total = 0.0;
    for (double value : values) {
      total += value;
    }
    return total;
  }

  private void applySwipeOffset(float offset) {
    mSwipeOffset = offset;
    layoutButtons();
    invalidate();
  }

  /*
   * Place the buttons for the row moved by mSwipeOffset. The side being revealed shows its
   * buttons stretched over the gap the row leaves. Past the full swipe point the first button
   * fills all of it.
   */
  private void layoutButtons() {
    mShownLeading = mSwipeOffset > 0f;
    double[] sizes = mShownLeading ? mLeadingSizes : mTrailingSizes;
    mShownCount = sizes.length;
    if (mSpans.length < 2 + mShownCount * 2) {
      mSpans = new double[2 + mShownCount * 2];
    }
    boolean full = mSwipe.isPastFullSwipe(mSwipeOffset);
    ShadowListSwipeReveal.buttonSpans(sizes, mShownCount, mSwipeOffset, full, getWidth(), mSpans);
  }

  private float buttonStart(int index) {
    return (float) mSpans[2 + index * 2];
  }

  private float buttonSize(int index) {
    return (float) mSpans[3 + index * 2];
  }

  // The shown button under x, or -1.
  private int buttonAt(float x) {
    for (int index = 0; index < mShownCount; index++) {
      float start = buttonStart(index);
      float size = buttonSize(index);
      if (size > 0f && x >= start && x < start + size) {
        return index;
      }
    }
    return -1;
  }

  /*
   * Let go: open a side, slide out for a full swipe, or close.
   */
  private void settleSwipe(float velocity) {
    double flingVelocity = ShadowListSwipeReveal.FLING_VELOCITY_DP * getResources().getDisplayMetrics().density;
    mSwipe.settle(mSwipeOffset, velocity, flingVelocity);
    float target = (float) mSwipe.restOffset();
    if (!mSwipe.isRestFull()) {
      animateSwipe(target, null);
      return;
    }
    boolean leading = mSwipe.restSide() == ShadowListSwipeReveal.SIDE_LEADING;
    mSwipedOut = true;
    // The row stays out until JS closes it with closeFullSwipe, or the action removes it.
    animateSwipe(target, () -> dispatchSwipeAction(leading, 0, true));
  }

  private void animateSwipe(float target, @Nullable Runnable onEnd) {
    cancelSwipeAnimator();
    ValueAnimator animator = ValueAnimator.ofFloat(mSwipeOffset, target);
    animator.setDuration(ShadowListSwipeReveal.SETTLE_DURATION_MS);
    animator.setInterpolator(new DecelerateInterpolator());
    animator.addUpdateListener(animation -> {
      if (mSwipeAnimator == animation) {
        applySwipeOffset((float) animation.getAnimatedValue());
      }
    });
    animator.addListener(new AnimatorListenerAdapter() {
      @Override
      public void onAnimationEnd(Animator animation) {
        // A cancelled animator was already replaced or cleared.
        if (mSwipeAnimator != animation) {
          return;
        }
        mSwipeAnimator = null;
        if (target == 0f) {
          tearDownSwipe();
        }
        if (onEnd != null) {
          onEnd.run();
        }
      }
    });
    mSwipeAnimator = animator;
    animator.start();
  }

  private void cancelSwipeAnimator() {
    ValueAnimator animator = mSwipeAnimator;
    mSwipeAnimator = null;
    if (animator != null) {
      animator.cancel();
    }
  }

  private void tearDownSwipe() {
    mSwipeOffset = 0f;
    mShownCount = 0;
    mSwipeTracking = false;
    mSwipedOut = false;
    // The row draws its buttons only while swiped. A background keeps drawing on.
    if (getBackground() == null) {
      setWillNotDraw(true);
    }
    invalidate();
    ShadowListView list = listView();
    if (list != null) {
      list.swipeRowClosed(this);
    }
  }

  /*
   * Run an action and close the row.
   */
  private void performSwipeAction(boolean leading, int actionIndex) {
    dispatchSwipeAction(leading, actionIndex, false);
    closeSwipeActions(true);
  }

  private void dispatchSwipeAction(boolean leading, int actionIndex, boolean fullSwipe) {
    EventDispatcher dispatcher = eventDispatcher();
    if (dispatcher != null) {
      dispatcher.dispatchEvent(ShadowListActionEvent.swipeAction(
        UIManagerHelper.getSurfaceId(this), getId(), leading, actionIndex, fullSwipe));
    }
  }

  /*
   * Draw the buttons in the gap, then the row's own drawing moved by the swipe offset.
   */
  @Override
  public void draw(Canvas canvas) {
    if (mSwipeOffset == 0f) {
      super.draw(canvas);
      return;
    }
    drawSwipeButtons(canvas);
    int saved = canvas.save();
    canvas.translate(mSwipeOffset, 0f);
    super.draw(canvas);
    canvas.restoreToCount(saved);
  }

  private void drawSwipeButtons(Canvas canvas) {
    ReadableArray actions = mShownLeading ? mLeadingSwipeActions : mTrailingSwipeActions;
    if (mShownCount == 0 || actionCount(actions) < mShownCount) {
      return;
    }
    float height = getHeight();
    float gapStart = (float) mSpans[0];
    float gap = (float) mSpans[1];
    int saved = canvas.save();
    // Only the gap shows the buttons' color.
    canvas.clipRect(gapStart, 0f, gapStart + gap, height);
    mButtonPaint.setColor(actionColor(actions, 0));
    canvas.drawRect(gapStart, 0f, gapStart + gap, height, mButtonPaint);
    float baseline = height / 2f - (mTextPaint.ascent() + mTextPaint.descent()) / 2f;
    for (int index = 0; index < mShownCount; index++) {
      float start = buttonStart(index);
      float size = buttonSize(index);
      if (size <= 0f) {
        continue;
      }
      mButtonPaint.setColor(actionColor(actions, index));
      canvas.drawRect(start, 0f, start + size, height, mButtonPaint);
      int button = canvas.save();
      canvas.clipRect(start, 0f, start + size, height);
      canvas.drawText(actionTitle(actions, index), start + size / 2f, baseline, mTextPaint);
      canvas.restoreToCount(button);
    }
    canvas.restoreToCount(saved);
  }

  private static int actionCount(@Nullable ReadableArray actions) {
    return actions != null ? actions.size() : 0;
  }

  private static String actionTitle(@Nullable ReadableArray actions, int index) {
    ReadableMap action = actions != null ? actions.getMap(index) : null;
    String title = action != null && action.hasKey("title") ? action.getString("title") : null;
    return title != null ? title : "";
  }

  private static boolean actionFlag(@Nullable ReadableArray actions, int index, String key) {
    ReadableMap action = actions != null ? actions.getMap(index) : null;
    return action != null && action.hasKey(key) && action.getBoolean(key);
  }

  /*
   * The processed ARGB color. A missing one falls back to red for a destructive action and
   * gray otherwise.
   */
  private static int actionColor(@Nullable ReadableArray actions, int index) {
    ReadableMap action = actions != null ? actions.getMap(index) : null;
    if (action != null && action.hasKey("color") && !action.isNull("color")) {
      return (int) (long) action.getDouble("color");
    }
    return actionFlag(actions, index, "destructive") ? DESTRUCTIVE_COLOR : Color.rgb(142, 142, 147);
  }

  // endregion

  // region Context menu

  @Nullable private String mContextMenuTitle = null;
  @Nullable private ReadableArray mContextMenuActions = null;

  public void setContextMenuTitle(@Nullable String title) {
    mContextMenuTitle = title;
  }

  public void setContextMenuActions(@Nullable ReadableArray actions) {
    mContextMenuActions = actions;
    updateAccessibilityActions();
  }

  boolean hasContextMenu() {
    return actionCount(mContextMenuActions) > 0;
  }

  /*
   * Show the row's menu for a long press. Returns whether there was one. PopupMenu has no
   * header and the title is not shown.
   */
  boolean showRowMenu(MotionEvent event) {
    int count = actionCount(mContextMenuActions);
    if (count == 0 || isSwipeOpen()) {
      return false;
    }
    PopupMenu popup = new PopupMenu(getContext(), this);
    Menu menu = popup.getMenu();
    for (int index = 0; index < count; index++) {
      CharSequence title = actionTitle(mContextMenuActions, index);
      if (actionFlag(mContextMenuActions, index, "destructive")) {
        SpannableString tinted = new SpannableString(title);
        tinted.setSpan(new ForegroundColorSpan(DESTRUCTIVE_COLOR), 0, tinted.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        title = tinted;
      }
      MenuItem item = menu.add(Menu.NONE, index, index, title);
      item.setEnabled(!actionFlag(mContextMenuActions, index, "disabled"));
    }
    popup.setOnMenuItemClickListener(item -> {
      dispatchContextMenuAction(item.getItemId());
      return true;
    });
    performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
    NativeGestureUtil.notifyNativeGestureStarted(this, event);
    popup.show();
    return true;
  }

  private void dispatchContextMenuAction(int actionIndex) {
    EventDispatcher dispatcher = eventDispatcher();
    if (dispatcher != null) {
      dispatcher.dispatchEvent(ShadowListActionEvent.contextMenuAction(
        UIManagerHelper.getSurfaceId(this), getId(), actionIndex));
    }
  }

  // endregion

  // region Accessibility

  // Ids of the swipe and menu actions added to the row.
  private final ArrayList<Integer> mAccessibilityActionIds = new ArrayList<>();

  /*
   * Each swipe action and each enabled menu action is a custom accessibility action on the
   * row.
   */
  private void updateAccessibilityActions() {
    for (int id : mAccessibilityActionIds) {
      ViewCompat.removeAccessibilityAction(this, id);
    }
    mAccessibilityActionIds.clear();
    addSwipeAccessibilityActions(mLeadingSwipeActions, true);
    addSwipeAccessibilityActions(mTrailingSwipeActions, false);
    for (int index = 0; index < actionCount(mContextMenuActions); index++) {
      if (actionFlag(mContextMenuActions, index, "disabled")) {
        continue;
      }
      int actionIndex = index;
      addAccessibilityAction(actionTitle(mContextMenuActions, index), () -> dispatchContextMenuAction(actionIndex));
    }
  }

  private void addSwipeAccessibilityActions(@Nullable ReadableArray actions, boolean leading) {
    for (int index = 0; index < actionCount(actions); index++) {
      int actionIndex = index;
      addAccessibilityAction(actionTitle(actions, index), () -> dispatchSwipeAction(leading, actionIndex, false));
    }
  }

  private void addAccessibilityAction(String label, Runnable perform) {
    int id = ViewCompat.addAccessibilityAction(this, label, (view, arguments) -> {
      perform.run();
      return true;
    });
    if (id != View.NO_ID) {
      mAccessibilityActionIds.add(id);
    }
  }

  // endregion

  @Nullable
  private EventDispatcher eventDispatcher() {
    return UIManagerHelper.getEventDispatcherForReactTag((ReactContext) getContext(), getId());
  }

  /*
   * Clear what a previous mount left behind before the view is reused.
   */
  void resetForRecycle() {
    mElementIndex = 0;
    mElementKey = "";
    resetSwipe();
    removeAllViews();
    ViewParent parent = getParent();
    if (parent instanceof ViewGroup) {
      ((ViewGroup) parent).removeView(this);
    }
  }

  private void resetSwipe() {
    cancelSwipeAnimator();
    mSwipeTouchClaimed = false;
    mButtonTouch = -1;
    if (mSwipeOffset != 0f || mSwipedOut) {
      tearDownSwipe();
    }
  }

  public ShadowListElementView(Context context, AttributeSet attrs) {
    super(context, attrs);
    init(context);
  }

  public ShadowListElementView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
    init(context);
  }

  private void init(Context context) {
    mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
    mTextPaint.setColor(Color.WHITE);
    mTextPaint.setTextAlign(Paint.Align.CENTER);
    mTextPaint.setTextSize(TypedValue.applyDimension(
      TypedValue.COMPLEX_UNIT_SP, BUTTON_TEXT_SP, context.getResources().getDisplayMetrics()));
    updateOutline();
  }

  /*
   * A row that leaves the window closes. Its native swipe goes with it.
   */
  @Override
  protected void onDetachedFromWindow() {
    resetSwipe();
    mSwipe.destroy();
    if (mVelocityTracker != null) {
      mVelocityTracker.recycle();
      mVelocityTracker = null;
    }
    super.onDetachedFromWindow();
  }

  /*
   * Drag to reorder lifts the held row with translationZ. Without a background the default
   * outline casts an invisible shadow that the renderer still processes every frame. Drop
   * the outline then. A row with a background keeps its outline and its lift shadow.
   */
  private void updateOutline() {
    setOutlineProvider(getBackground() == null ? null : ViewOutlineProvider.BACKGROUND);
  }

  @Override
  @SuppressWarnings("deprecation")
  public void setBackgroundDrawable(@Nullable Drawable background) {
    super.setBackgroundDrawable(background);
    updateOutline();
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    // The core positions the children.
  }
}
