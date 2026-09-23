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

  /*
   * Hide on scroll. The hidden values are how far each bar has slid away in pixels.
   * The last offset gives the scroll distance since the previous tick.
   */
  private boolean mAutoHideHeader = false;
  private boolean mAutoHideFooter = false;
  private float mHeaderHidden = 0f;
  private float mFooterHidden = 0f;
  private float mLastAutoHideOffset = 0f;

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
    mHeaderHidden = 0f;
    mFooterHidden = 0f;
    mLastAutoHideOffset = 0f;
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

    // Measure the header and footer first so one can push the other away when they meet.
    View header = mHeaderView;
    View footer = mFooterView;
    float headerSize = header == null ? 0f : (horizontal ? header.getWidth() : header.getHeight());
    float footerSize = footer == null ? 0f : (horizontal ? footer.getWidth() : footer.getHeight());

    float axisOffset = horizontal ? offsetX : offsetY;
    float windowSize = horizontal ? windowWidth : windowHeight;
    float contentSize = horizontal ? contentWidth : contentHeight;

    // How far the user scrolled since the last tick. Zero for programmatic scrolls.
    float autoHideDelta = accumulate ? (axisOffset - mLastAutoHideOffset) : 0f;
    mLastAutoHideOffset = axisOffset;

    // The two bars don't read each other's result, so the order doesn't matter.
    for (int pass = 0; pass < 2; pass++) {
      boolean isFooter = pass == 1;
      View child = isFooter ? footer : header;
      if (child == null) {
        continue;
      }

      boolean autoHide = isFooter ? mAutoHideFooter : mAutoHideHeader;
      boolean sticky = isFooter ? mStickyFooter : mStickyHeader;

      float translation = 0f;
      if (isFooter) {
        float footerStart = horizontal ? child.getLeft() : child.getTop();
        float restingTranslation = (axisOffset + windowSize - footerSize) - footerStart;
        if (mAutoHideFooter) {
          // Pin to the bottom, slid down by the hidden amount. Always shown near the end.
          float maxOffset = Math.max(0f, contentSize - windowSize);
          if (axisOffset >= maxOffset - footerSize) {
            mFooterHidden = 0f;
          } else {
            mFooterHidden = Math.max(0f, Math.min(mFooterHidden + autoHideDelta, footerSize));
          }
          translation = restingTranslation + mFooterHidden;
        } else if (mStickyFooter) {
          translation = restingTranslation;
        }
      } else {
        if (mAutoHideHeader) {
          // Pin to the top, slid up by the hidden amount. Always shown near the start.
          if (axisOffset <= headerSize) {
            mHeaderHidden = 0f;
          } else {
            mHeaderHidden = Math.max(0f, Math.min(mHeaderHidden + autoHideDelta, headerSize));
          }
          translation = axisOffset - mHeaderHidden;
        } else if (mStickyHeader) {
          // Pin to the top, and let the end of the content push it away.
          translation = axisOffset;
          float collisionTop = contentSize - footerSize - headerSize;
          if (collisionTop < translation) {
            translation = collisionTop;
          }
        }
      }

      if (horizontal) {
        child.setTranslationX(translation);
        child.setTranslationY(0f);
      } else {
        child.setTranslationY(translation);
        child.setTranslationX(0f);
      }
      // Lift a pinned or hiding bar above the rows.
      child.setTranslationZ((sticky || autoHide) ? stickyLift : 0f);
    }

    applyStickySectionHeaders();
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
    double axisOffsetPx = horizontal ? scrollView.getScrollX() : scrollView.getScrollY();
    if (axisOffsetPx < 0.0) {
      axisOffsetPx = 0.0;
    }
    double axisOffset = PixelUtil.toDIPFromPixel((float) axisOffsetPx);

    /*
     * Headers are sorted by offset. The active one is the last at or above the top,
     * the next one is the first below it.
     */
    boolean hasActive = false;
    double activeSize = 0.0;
    boolean hasNext = false;
    double nextOffset = 0.0;
    for (int i = 0; i < mStickyHeaderOffsets.length; i++) {
      double headerOffset = mStickyHeaderOffsets[i];
      if (headerOffset <= axisOffset) {
        hasActive = true;
        activeSize = mStickyHeaderSizes[i];
      } else {
        nextOffset = headerOffset;
        hasNext = true;
        break;
      }
    }

    if (!hasActive) {
      overlay.setVisibility(View.GONE);
      return;
    }

    // Sit at the top unless the next header is pushing it up.
    double translation = axisOffset;
    if (hasNext) {
      double pushedTop = nextOffset - activeSize;
      if (pushedTop < translation) {
        translation = pushedTop;
      }
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
