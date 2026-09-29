package com.shadowlist;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.uimanager.PixelUtil;

/*
 * Pins the header, footer and section header overlay. Runs on the UI thread on every
 * scroll tick, using the core state cached here.
 */
class ShadowListStickyController {
  /*
   * Pinned views are lifted with Z. Never use bringToFront, it reorders the children and
   * breaks index based mounting.
   */
  private static final int STICKY_LIFT_DP = 4;
  private static final int SECTION_OVERLAY_LIFT_DP = 6;

  private final ShadowListView mView;

  private boolean mStickyHeader = false;
  private boolean mStickyFooter = false;

  // Hide on scroll.
  private boolean mAutoHideHeader = false;
  private boolean mAutoHideFooter = false;

  /*
   * Inputs and results of the pinning math, see ShadowListGeometry.STICKY_*. Its state slots
   * keep how far each auto hide bar has slid away, in pixels, across ticks.
   */
  private final double[] mStickySlots = new double[ShadowListGeometry.STICKY_SLOTS];

  /*
   * Where each section header sits in the list and how big it is, in dp along the scroll
   * axis. Comes from the core and is read on every scroll tick.
   */
  private int[] mStickyHeaderIndices = new int[0];
  private double[] mStickyHeaderOffsets = new double[0];
  private double[] mStickyHeaderSizes = new double[0];

  /*
   * The header, footer and section header overlay, found once per mount change instead
   * of scanning every child on every scroll tick. Null when the list has none.
   */
  private @Nullable View mHeaderView = null;
  private @Nullable View mFooterView = null;
  private @Nullable View mOverlayView = null;
  private boolean mTemplatesDirty = true;

  ShadowListStickyController(ShadowListView view) {
    mView = view;
  }

  void setStickyHeader(boolean stickyHeader) {
    mStickyHeader = stickyHeader;
    applyStickyTransforms();
  }

  void setStickyFooter(boolean stickyFooter) {
    mStickyFooter = stickyFooter;
    applyStickyTransforms();
  }

  void setAutoHideHeader(boolean autoHideHeader) {
    mAutoHideHeader = autoHideHeader;
    applyStickyTransforms();
  }

  void setAutoHideFooter(boolean autoHideFooter) {
    mAutoHideFooter = autoHideFooter;
    applyStickyTransforms();
  }

  /*
   * Reset hide on scroll so a recycled view starts clean.
   */
  void reset() {
    mStickySlots[ShadowListGeometry.STICKY_HEADER_HIDDEN] = 0.0;
    mStickySlots[ShadowListGeometry.STICKY_FOOTER_HIDDEN] = 0.0;
    mStickySlots[ShadowListGeometry.STICKY_LAST_OFFSET] = 0.0;
    invalidateTemplates();
  }

  /*
   * A template mounted, unmounted or changed type. Find the sticky views again on the
   * next tick.
   */
  void invalidateTemplates() {
    mTemplatesDirty = true;
    mHeaderView = null;
    mFooterView = null;
    mOverlayView = null;
  }

  /*
   * Rescan when marked dirty or when a cached view left the content view without a
   * remove call, for example when it was recycled with its list.
   */
  private void resolveTemplates(ViewGroup contentView) {
    if (!mTemplatesDirty
        && (mHeaderView == null || mHeaderView.getParent() == contentView)
        && (mFooterView == null || mFooterView.getParent() == contentView)
        && (mOverlayView == null || mOverlayView.getParent() == contentView)) {
      return;
    }
    mTemplatesDirty = false;
    mHeaderView = null;
    mFooterView = null;
    mOverlayView = null;
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowListTemplateView)) {
        continue;
      }
      String type = ((ShadowListTemplateView) child).getTemplateType();
      if ("footer".equals(type)) {
        mFooterView = child;
      } else if ("header".equals(type)) {
        mHeaderView = child;
      } else if ("sectionHeader".equals(type) && mOverlayView == null) {
        mOverlayView = child;
      }
      // Other templates like empty are never pinned.
    }
  }

  /*
   * Save the section header positions sent by the core.
   */
  void cacheStickyGeometry(ReadableMap nextStateData) {
    if (nextStateData.hasKey("stickyHeaderIndices")
        && nextStateData.hasKey("stickyHeaderOffsets")
        && nextStateData.hasKey("stickyHeaderSizes")) {
      ReadableArray indices = nextStateData.getArray("stickyHeaderIndices");
      ReadableArray offsets = nextStateData.getArray("stickyHeaderOffsets");
      ReadableArray sizes = nextStateData.getArray("stickyHeaderSizes");
      int count = indices != null ? indices.size() : 0;
      mStickyHeaderIndices = new int[count];
      mStickyHeaderOffsets = new double[count];
      mStickyHeaderSizes = new double[count];
      for (int i = 0; i < count; i++) {
        mStickyHeaderIndices[i] = indices.getInt(i);
        mStickyHeaderOffsets[i] = offsets != null ? offsets.getDouble(i) : 0.0;
        mStickyHeaderSizes[i] = sizes != null ? sizes.getDouble(i) : 0.0;
      }
    }
  }

  /*
   * Pin the header and footer by moving them from where they normally sit.
   */
  void applyStickyTransforms() {
    applyStickyTransforms(false);
  }

  /*
   * Pass accumulate only for real user scrolls. Programmatic offset fixes must not
   * hide or show the bars.
   */
  void applyStickyTransforms(boolean accumulate) {
    ViewGroup contentView = mView.getContentView();
    ViewGroup scrollView = mView.getScrollView();
    if (contentView == null || scrollView == null) {
      return;
    }
    boolean horizontal = mView.isHorizontal();

    int offsetX = scrollView.getScrollX();
    int offsetY = scrollView.getScrollY();
    int windowWidth = scrollView.getWidth();
    int windowHeight = scrollView.getHeight();
    int contentWidth = contentView.getWidth();
    int contentHeight = contentView.getHeight();

    float stickyLift = PixelUtil.toPixelFromDIP(STICKY_LIFT_DP);

    resolveTemplates(contentView);

    View header = mHeaderView;
    View footer = mFooterView;
    double[] slots = mStickySlots;
    slots[ShadowListGeometry.STICKY_OFFSET] = horizontal ? offsetX : offsetY;
    slots[ShadowListGeometry.STICKY_WINDOW_SIZE] = horizontal ? windowWidth : windowHeight;
    slots[ShadowListGeometry.STICKY_CONTENT_SIZE] = horizontal ? contentWidth : contentHeight;
    slots[ShadowListGeometry.STICKY_HAS_HEADER] = header != null ? 1.0 : 0.0;
    slots[ShadowListGeometry.STICKY_HEADER_SIZE] =
      header == null ? 0.0 : (horizontal ? header.getWidth() : header.getHeight());
    slots[ShadowListGeometry.STICKY_STICKY_HEADER] = mStickyHeader ? 1.0 : 0.0;
    slots[ShadowListGeometry.STICKY_AUTO_HIDE_HEADER] = mAutoHideHeader ? 1.0 : 0.0;
    slots[ShadowListGeometry.STICKY_HAS_FOOTER] = footer != null ? 1.0 : 0.0;
    slots[ShadowListGeometry.STICKY_FOOTER_SIZE] =
      footer == null ? 0.0 : (horizontal ? footer.getWidth() : footer.getHeight());
    // The footer's real place in the content, without any translation.
    slots[ShadowListGeometry.STICKY_FOOTER_START] =
      footer == null ? 0.0 : (horizontal ? footer.getLeft() : footer.getTop());
    slots[ShadowListGeometry.STICKY_STICKY_FOOTER] = mStickyFooter ? 1.0 : 0.0;
    slots[ShadowListGeometry.STICKY_AUTO_HIDE_FOOTER] = mAutoHideFooter ? 1.0 : 0.0;
    // Only real user scrolls slide the auto hide bars.
    slots[ShadowListGeometry.STICKY_ACCUMULATE] = accumulate ? 1.0 : 0.0;
    ShadowListGeometry.stickyTranslations(slots);

    if (header != null) {
      placeBar(header, horizontal, (float) slots[ShadowListGeometry.STICKY_HEADER_TRANSLATION],
        mStickyHeader || mAutoHideHeader, stickyLift);
    }
    if (footer != null) {
      placeBar(footer, horizontal, (float) slots[ShadowListGeometry.STICKY_FOOTER_TRANSLATION],
        mStickyFooter || mAutoHideFooter, stickyLift);
    }

    applyStickySectionHeaders();
  }

  private static void placeBar(View child, boolean horizontal, float translation, boolean lifted, float lift) {
    if (horizontal) {
      child.setTranslationX(translation);
      child.setTranslationY(0f);
    } else {
      child.setTranslationY(translation);
      child.setTranslationX(0f);
    }
    // Lift a pinned or hiding bar above the rows.
    child.setTranslationZ(lifted ? lift : 0f);
  }

  /*
   * Pin the section header overlay to the top and let the next header push it up.
   * Hide it when no section is active.
   */
  private void applyStickySectionHeaders() {
    ViewGroup contentView = mView.getContentView();
    ViewGroup scrollView = mView.getScrollView();
    if (contentView == null || scrollView == null) {
      return;
    }

    resolveTemplates(contentView);
    View overlay = mOverlayView;
    if (overlay == null) {
      return;
    }

    if (mStickyHeaderIndices.length == 0) {
      overlay.setVisibility(View.GONE);
      return;
    }

    boolean horizontal = mView.isHorizontal();
    double axisOffset = PixelUtil.toDIPFromPixel(horizontal ? scrollView.getScrollX() : scrollView.getScrollY());
    double translation = ShadowListGeometry.sectionOverlayTranslation(
      mStickyHeaderOffsets, mStickyHeaderSizes, mStickyHeaderOffsets.length, axisOffset);
    if (Double.isNaN(translation)) {
      overlay.setVisibility(View.GONE);
      return;
    }
    float translationPx = PixelUtil.toPixelFromDIP((float) translation);

    overlay.setVisibility(View.VISIBLE);
    if (horizontal) {
      overlay.setTranslationX(translationPx);
      overlay.setTranslationY(0f);
    } else {
      overlay.setTranslationY(translationPx);
      overlay.setTranslationX(0f);
    }
    // Keep the overlay above the rows and above a sticky header.
    overlay.setTranslationZ(PixelUtil.toPixelFromDIP(SECTION_OVERLAY_LIFT_DP));
  }
}
