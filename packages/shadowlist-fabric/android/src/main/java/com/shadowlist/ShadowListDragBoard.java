package com.shadowlist;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Outline;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.Choreographer;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import com.facebook.react.uimanager.PixelUtil;
import com.facebook.soloader.SoLoader;

import java.util.ArrayList;
import java.util.HashMap;

/*
 * The lists that share one dragBoardId, and the row being dragged between them, the Android
 * side of iOS's ShadowListDragBoard. The registry finds a board by id, made on first use, and
 * forgets it when its last list leaves. A list joins with its drag controller and leaves when
 * its id changes or Fabric drops it. UI thread only.
 *
 * A drag starts in its home list as usual. The core's DragBoard, reached through
 * ShadowListDragBoardJNI, decides each frame whether the row lifts out, comes back home or moves
 * to another target, where its floating picture goes, and how long a drop waits to land. Once the
 * finger is over another list of the board, the row's picture floats above the window as the
 * overlay, a view added to the window's root view, and the home list closes its gap. The list
 * the row hovers is the target and opens a gap for it. A drop there sends one onDragEnd from the
 * home list, and the overlay waits for the row to show up in the target's data before it
 * settles onto it.
 *
 * The fields below the registry are the views of the current flight, which outlives the drag
 * until the row settles: the home list the row came from, the target it hovers or dropped in,
 * the overlay, the root view's size at pickup, the scroll view around the board the drag
 * scrolled, and whether the picture is settling onto the landed row.
 */
final class ShadowListDragBoard {
  static {
    SoLoader.loadLibrary("react_codegen_ShadowListViewSpec");
  }

  /*
   * What a hover changes, matching DragBoardStep::Kind.
   */
  static final int STEP_STAY = 0;
  static final int STEP_LIFT_OUT = 1;
  static final int STEP_RETURN_HOME = 2;
  static final int STEP_RETARGET = 3;

  /*
   * How a release ends, matching DragBoardRelease::Kind.
   */
  static final int RELEASE_HOME = 0;
  static final int RELEASE_CANCEL = 1;
  static final int RELEASE_DROP = 2;

  /*
   * Where the held row is, matching DragBoardPhase.
   */
  static final int PHASE_IDLE = 0;
  static final int PHASE_AT_HOME = 1;
  static final int PHASE_FLOATING = 2;
  static final int PHASE_LANDING = 3;
  static final int PHASE_SETTLING = 4;

  /*
   * Slots of the array frame fills. Match FrameSlot in ShadowListDragBoardJNI.cpp.
   */
  static final int FRAME_FLOATING = 0;
  static final int FRAME_OVERLAY_X = 1;
  static final int FRAME_OVERLAY_Y = 2;
  static final int FRAME_HIDE_HELD = 3;
  private static final int FRAME_SLOTS = 4;

  /*
   * How long a drop into another list waits for its row to land, from the core.
   */
  static final long LAND_TIMEOUT_MS;

  /*
   * Slots of the array nativeConstants fills. Match ConstantSlot in ShadowListDragBoardJNI.cpp.
   */
  private static final int CONSTANT_LAND_TIMEOUT_MS = 0;
  private static final int CONSTANT_SLOTS = 1;

  /*
   * How long past the landing timeout a picture that never found its row waits before the
   * flight ends anyway.
   */
  private static final long SETTLE_GRACE_MS = 50;

  static {
    double[] constants = new double[CONSTANT_SLOTS];
    nativeConstants(constants);
    LAND_TIMEOUT_MS = Math.round(constants[CONSTANT_LAND_TIMEOUT_MS]);
  }

  private static final ShadowListPeerReclaimer RECLAIMER =
    new ShadowListPeerReclaimer(ShadowListDragBoard::nativeDestroy);

  private static final HashMap<String, ShadowListDragBoard> BOARDS = new HashMap<>();

  static ShadowListDragBoard boardWithId(String boardId) {
    ShadowListDragBoard board = BOARDS.get(boardId);
    if (board == null) {
      board = new ShadowListDragBoard(boardId);
      BOARDS.put(boardId, board);
    }
    return board;
  }

  private final String mBoardId;
  private final ArrayList<ShadowListDragController> mLists = new ArrayList<>();
  private int mNextListToken = 0;
  private long mHandle;

  @Nullable private ShadowListDragController mHome = null;
  @Nullable private ShadowListDragController mTarget = null;
  @Nullable private View mOverlay = null;
  private int mOverlayWidth = 0;
  private int mOverlayHeight = 0;
  private int mWindowWidth = 0;
  private int mWindowHeight = 0;
  @Nullable private ViewGroup mSidewaysScrollView = null;
  private boolean mSettling = false;
  /*
   * The list a drag the core is deciding started in, and when the last drop went out.
   */
  @Nullable private ShadowListDragController mPickupList = null;
  private long mDropAtMs = 0L;
  @Nullable private Choreographer.FrameCallback mLandingCallback = null;

  private final double[] mFrame = new double[FRAME_SLOTS];
  private final double[] mOffset = new double[2];
  private String[] mCandidates = new String[0];
  private final ArrayList<ShadowListDragController> mHits = new ArrayList<>();
  private final Rect mRect = new Rect();
  private final int[] mLocation = new int[2];

  private ShadowListDragBoard(String boardId) {
    mBoardId = boardId;
  }

  private long handle() {
    if (mHandle == 0) {
      mHandle = nativeCreate();
      RECLAIMER.register(this, mHandle);
    }
    return mHandle;
  }

  private void destroy() {
    if (mHandle == 0) {
      return;
    }
    nativeDestroy(mHandle);
    RECLAIMER.release(mHandle);
    mHandle = 0;
  }

  // region Membership

  /*
   * A list joins and gets its core id, unique on the board where dragListId may not be.
   */
  String join(ShadowListDragController list) {
    mLists.add(list);
    mNextListToken += 1;
    listsChanged();
    return Integer.toString(mNextListToken);
  }

  /*
   * A list leaves. The last list to leave takes the board out of the registry.
   */
  void leave(ShadowListDragController list) {
    mLists.remove(list);
    if (mPickupList == list) {
      mPickupList = null;
    }
    if (mLists.isEmpty()) {
      stopLandingLoop();
      if (BOARDS.get(mBoardId) == this) {
        BOARDS.remove(mBoardId);
      }
      destroy();
      return;
    }
    listsChanged();
  }

  ArrayList<ShadowListDragController> getLists() {
    return mLists;
  }

  /*
   * A list joined or left, or one changed its id, label, drag switch or window. Every row's
   * TalkBack Move to actions name the lists it can move to.
   */
  void listsChanged() {
    for (ShadowListDragController list : mLists) {
      list.updateCellAccessibilityActions();
    }
  }

  @Nullable ShadowListDragController listWithToken(String token) {
    if (token == null || token.isEmpty()) {
      return null;
    }
    for (ShadowListDragController list : mLists) {
      if (token.equals(list.getBoardToken())) {
        return list;
      }
    }
    return null;
  }

  // endregion

  // region Flight

  void begin(ShadowListDragController home, String heldKey, float fingerX, float fingerY, float centerX, float centerY,
    int windowWidth, int windowHeight) {
    nativeBegin(handle(), home.getBoardToken(), heldKey, fingerX, fingerY, centerX, centerY);
    mPickupList = home;
    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;
    mSidewaysScrollView = null;
  }

  int frame(float fingerX, float fingerY, float shownX, float shownY, int candidateCount, double nowMs) {
    return nativeFrame(handle(), fingerX, fingerY, shownX, shownY, mCandidates, candidateCount, nowMs, mFrame);
  }

  double[] getFrame() {
    return mFrame;
  }

  String getStepNext() {
    return nativeStepNext(handle());
  }

  /*
   * The held row's offset after a return home, in window pixels. The array is reused.
   */
  double[] returnOffset(float placedX, float placedY, double nowMs) {
    nativeReturnOffset(handle(), placedX, placedY, nowMs, mOffset);
    return mOffset;
  }

  int release(int insertionIndex, String insertionKey, int rowCount, double nowMs) {
    return nativeRelease(handle(), insertionIndex, insertionKey, rowCount, nowMs);
  }

  String getReleaseListId() {
    return nativeReleaseListId(handle());
  }

  int getReleaseIndex() {
    return nativeReleaseIndex(handle());
  }

  String getReleaseKey() {
    return nativeReleaseKey(handle());
  }

  boolean landed(String listId, boolean keysHoldHeldKey) {
    return nativeLanded(handle(), listId, keysHoldHeldKey);
  }

  /*
   * The drag stops without a drop. Returns the target's core id, empty for none.
   */
  String cancelCore() {
    mPickupList = null;
    return nativeCancel(handle());
  }

  void endCore() {
    mPickupList = null;
    nativeEndFlight(handle());
  }

  void leaveCore(String listId) {
    nativeLeave(handle(), listId);
    if (getPhase() == PHASE_IDLE) {
      mPickupList = null;
    }
  }

  int getPhase() {
    return mHandle == 0 ? PHASE_IDLE : nativePhase(mHandle);
  }

  boolean isDeciding() {
    int phase = getPhase();
    return phase == PHASE_AT_HOME || phase == PHASE_FLOATING;
  }

  boolean isFloating() {
    return getPhase() == PHASE_FLOATING;
  }

  boolean isLanding() {
    int phase = getPhase();
    return phase == PHASE_LANDING || phase == PHASE_SETTLING;
  }

  /*
   * Whether list is the home of a drag the core is still deciding, before its drop.
   */
  boolean isDecidingFor(ShadowListDragController list) {
    return mPickupList == list && isDeciding();
  }

  String getHeldKey() {
    return mHandle == 0 ? "" : nativeHeldKey(mHandle);
  }

  String getTargetId() {
    return mHandle == 0 ? "" : nativeTargetId(mHandle);
  }

  long getFlight() {
    return mHandle == 0 ? 0L : nativeFlight(mHandle);
  }

  double scrollDelta(double finger, double touch, double windowSize, double dtSeconds) {
    return nativeScrollDelta(handle(), finger, touch, windowSize, PixelUtil.toPixelFromDIP(1f), dtSeconds);
  }

  @Nullable ShadowListDragController getHome() {
    return mHome;
  }

  void setHome(@Nullable ShadowListDragController home) {
    mHome = home;
  }

  @Nullable ShadowListDragController getTarget() {
    return mTarget;
  }

  void setTarget(@Nullable ShadowListDragController target) {
    mTarget = target;
  }

  boolean isSettling() {
    return mSettling;
  }

  boolean windowSizeChanged(int width, int height) {
    return width != mWindowWidth || height != mWindowHeight;
  }

  @Nullable ViewGroup takeSidewaysScrollView() {
    ViewGroup outer = mSidewaysScrollView;
    mSidewaysScrollView = null;
    return outer;
  }

  void setSidewaysScrollView(ViewGroup outer) {
    mSidewaysScrollView = outer;
  }

  /*
   * End the flight right away, without animation: drop the overlay, show the hidden rows and
   * clear the gaps in the home list and the target. A drag still deciding with the core stops
   * there too.
   */
  void endFlight() {
    ShadowListDragController home = mHome;
    ShadowListDragController target = mTarget;
    removeOverlay();
    mHome = null;
    mTarget = null;
    mSettling = false;
    stopLandingLoop();
    endCore();
    if (home != null) {
      home.boardFlightDidEnd();
    }
    if (target != null && target != home) {
      target.boardFlightDidEnd();
    }
  }

  // endregion

  // region Hit test

  /*
   * The core ids of the lists of this board under a point in the window, for the core's hover,
   * topmost first, written into the candidates the next frame reads. Returns their count. A
   * list counts where its visible rect holds the point, which leaves out the part an outer
   * scroll view clips. Lists off the window, hidden, with drag off, busy with a drag of their
   * own, or without a dragListId to name them to JS are left out. Nested lists come deepest
   * first, and the core picks the home list where lists overlap.
   */
  int candidatesAt(float x, float y, ShadowListDragController self) {
    mHits.clear();
    for (ShadowListDragController list : mLists) {
      if (!list.canTakeBoardRow(list == self)) {
        continue;
      }
      ShadowListView view = list.getView();
      // A list in another window, like a modal's, is not under this finger.
      if (view.getRootView() != self.getView().getRootView() || !view.getGlobalVisibleRect(mRect)) {
        continue;
      }
      // Global rects are in the root view's space. Move them to the window's.
      view.getRootView().getLocationInWindow(mLocation);
      mRect.offset(mLocation[0], mLocation[1]);
      if (mRect.contains(Math.round(x), Math.round(y))) {
        mHits.add(list);
      }
    }
    mHits.sort((first, second) -> Integer.compare(depthOf(second.getView()), depthOf(first.getView())));
    if (mCandidates.length < mHits.size()) {
      mCandidates = new String[mHits.size()];
    }
    for (int position = 0; position < mHits.size(); position++) {
      mCandidates[position] = mHits.get(position).getBoardToken();
    }
    int count = mHits.size();
    mHits.clear();
    return count;
  }

  private static int depthOf(View view) {
    int depth = 0;
    for (Object parent = view.getParent(); parent instanceof View; parent = ((View) parent).getParent()) {
      depth++;
    }
    return depth;
  }

  // endregion

  // region Overlay

  @Nullable View getOverlay() {
    return mOverlay;
  }

  /*
   * The row's picture above the window, where the row shows now. The picture is the row as it
   * draws, without its lift shadow. The overlay has its own: the same shape as the row's, see
   * ShadowListCellView.getLiftOutline. Returns false when the window has no root view to hold it.
   */
  boolean showOverlay(ShadowListCellView cell, float windowLeft, float windowTop) {
    removeOverlay();
    View root = cell.getRootView();
    int width = cell.getWidth();
    int height = cell.getHeight();
    if (!(root instanceof ViewGroup) || width <= 0 || height <= 0) {
      return false;
    }
    Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    cell.draw(new Canvas(bitmap));

    ImageView overlay = new ImageView(cell.getContext());
    overlay.setImageBitmap(bitmap);
    overlay.setScaleType(ImageView.ScaleType.FIT_XY);
    overlay.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
    overlay.setClickable(false);
    overlay.setFocusable(false);
    overlay.setTag("shadowlist-drag-board-overlay");
    Outline shape = new Outline();
    ShadowListCellView.getLiftOutline(cell, shape);
    overlay.setOutlineProvider(new ViewOutlineProvider() {
      @Override
      public void getOutline(View view, Outline outline) {
        outline.set(shape);
      }
    });
    overlay.setElevation(PixelUtil.toPixelFromDIP((float) ShadowListDrag.LIFT_ELEVATION_DP));
    ((ViewGroup) root).addView(overlay, new FrameLayout.LayoutParams(width, height));
    mOverlay = overlay;
    mOverlayWidth = width;
    mOverlayHeight = height;
    moveOverlayTo(windowLeft, windowTop);
    return true;
  }

  /*
   * Put the overlay's top left corner at a point in the window. The root view lays it out at
   * its own padding.
   */
  void moveOverlayTo(float windowLeft, float windowTop) {
    View overlay = mOverlay;
    if (overlay == null || !(overlay.getParent() instanceof ViewGroup)) {
      return;
    }
    ViewGroup root = (ViewGroup) overlay.getParent();
    root.getLocationInWindow(mLocation);
    overlay.setTranslationX(windowLeft - mLocation[0] - root.getPaddingLeft());
    overlay.setTranslationY(windowTop - mLocation[1] - root.getPaddingTop());
  }

  /*
   * The overlay is not laid out until the next layout pass. Its size comes from the row.
   */
  void moveOverlayCenterTo(float centerX, float centerY) {
    moveOverlayTo(centerX - mOverlayWidth / 2f, centerY - mOverlayHeight / 2f);
  }

  /*
   * Slide the overlay to a top left corner in the window over the drop's duration, then run
   * endAction.
   */
  void animateOverlayTo(View overlay, float windowLeft, float windowTop, Runnable endAction) {
    if (!(overlay.getParent() instanceof ViewGroup)) {
      endAction.run();
      return;
    }
    ViewGroup root = (ViewGroup) overlay.getParent();
    root.getLocationInWindow(mLocation);
    overlay.animate().cancel();
    overlay.animate()
      .translationX(windowLeft - mLocation[0] - root.getPaddingLeft())
      .translationY(windowTop - mLocation[1] - root.getPaddingTop())
      .setDuration(ShadowListDrag.DROP_DURATION_MS)
      .withEndAction(endAction)
      .start();
  }

  /*
   * Hand the overlay over to an animation that removes it when done. The flight no longer
   * holds it.
   */
  @Nullable View detachOverlay() {
    View overlay = mOverlay;
    mOverlay = null;
    return overlay;
  }

  static void removeOverlayView(@Nullable View overlay) {
    if (overlay == null) {
      return;
    }
    overlay.animate().cancel();
    if (overlay.getParent() instanceof ViewGroup) {
      ((ViewGroup) overlay.getParent()).removeView(overlay);
    }
  }

  void removeOverlay() {
    removeOverlayView(detachOverlay());
  }

  /*
   * The picture flies onto the landed row, and the flight ends when it arrives.
   */
  void settleOverlay(float windowLeft, float windowTop) {
    View overlay = mOverlay;
    if (overlay == null) {
      endFlight();
      return;
    }
    mSettling = true;
    long flight = getFlight();
    animateOverlayTo(overlay, windowLeft, windowTop, () -> {
      if (getFlight() == flight && mSettling) {
        endFlight();
      }
    });
  }

  // endregion

  // region Landing

  /*
   * After a drop into another list, check every frame until the flight ends: the home list
   * clears its gap once the row left its data, the target ends its own once its data holds the
   * row, and the picture settles onto the row once its cell is laid out. The move may never land,
   * like when JS has no onMoveItemToList. A drop still waiting expires after the core's landing
   * timeout, and a row that landed off screen never mounts to settle on.
   */
  void startLandingLoop() {
    mDropAtMs = SystemClock.uptimeMillis();
    if (mLandingCallback == null) {
      mLandingCallback = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long frameTimeNanos) {
          landingFrame();
        }
      };
    }
    Choreographer.getInstance().removeFrameCallback(mLandingCallback);
    Choreographer.getInstance().postFrameCallback(mLandingCallback);
  }

  private void stopLandingLoop() {
    if (mLandingCallback != null) {
      Choreographer.getInstance().removeFrameCallback(mLandingCallback);
    }
  }

  private void landingFrame() {
    long now = SystemClock.uptimeMillis();
    int phase = getPhase();
    if (phase == PHASE_LANDING && nativeExpired(handle(), now)) {
      endFlight();
      return;
    }
    if (phase == PHASE_SETTLING && !mSettling && now - mDropAtMs > LAND_TIMEOUT_MS + SETTLE_GRACE_MS) {
      endFlight();
      return;
    }
    if (!isLanding()) {
      return;
    }
    ShadowListDragController home = mHome;
    ShadowListDragController target = mTarget;
    if (home != null) {
      home.boardMountDidFinish(true);
    }
    if (target != null && target != home) {
      target.boardMountDidFinish(true);
    }
    if (isLanding() && !mSettling && mLandingCallback != null) {
      Choreographer.getInstance().postFrameCallback(mLandingCallback);
    }
  }

  // endregion

  private static native long nativeCreate();
  private static native void nativeDestroy(long handle);
  private static native void nativeConstants(double[] out);
  private static native void nativeBegin(
    long handle, String homeId, String heldKey, double fingerX, double fingerY, double heldCenterX,
    double heldCenterY);
  private static native int nativeFrame(
    long handle, double fingerX, double fingerY, double shownX, double shownY, String[] candidates, int count,
    double nowMs, double[] out);
  private static native String nativeStepNext(long handle);
  private static native void nativeReturnOffset(long handle, double placedX, double placedY, double nowMs, double[] out);
  private static native int nativeRelease(long handle, int insertionIndex, String insertionKey, int rowCount, double nowMs);
  private static native String nativeReleaseListId(long handle);
  private static native int nativeReleaseIndex(long handle);
  private static native String nativeReleaseKey(long handle);
  private static native boolean nativeLanded(long handle, String listId, boolean keysHoldHeldKey);
  private static native boolean nativeExpired(long handle, double nowMs);
  private static native String nativeCancel(long handle);
  private static native void nativeEndFlight(long handle);
  private static native void nativeLeave(long handle, String listId);
  private static native int nativePhase(long handle);
  private static native String nativeHeldKey(long handle);
  private static native String nativeTargetId(long handle);
  private static native long nativeFlight(long handle);
  private static native double nativeScrollDelta(
    long handle, double finger, double touch, double windowSize, double pixelsPerDp, double dtSeconds);
}
