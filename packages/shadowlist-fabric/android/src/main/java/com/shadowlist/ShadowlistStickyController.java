package com.shadowlist;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.uimanager.PixelUtil;

/*
 * Native sticky pinning for the header, footer and section-header overlay, split out of
 * ShadowlistView. The geometry runs on the UI thread every scroll tick off the
 * core-published state the host caches here (mirroring Container::resolveStickyHeader),
 * so it stays in lockstep with the gesture without the commit-cycle latency that made
 * the core-driven version choppy. The host view owns the scroll/content views; this
 * controller reaches them through the ShadowlistView accessors.
 */
class ShadowlistStickyController {
  /*
   * Lift the pinned views above the scrolling rows via Z (draw order), NOT
   * bringToFront(): bringToFront() reorders mContentView's child array, which desyncs
   * Fabric's index-based child mounting (getContentChildAt / removeContentViewAt) and
   * corrupts the view tree - the same hazard ShadowlistDragController documents and
   * avoids. The section-header overlay sits above a plain sticky header, both above
   * the rows; the dragged row (8dp) stays above all.
   */
  private static final int STICKY_LIFT_DP = 4;
  private static final int SECTION_OVERLAY_LIFT_DP = 6;

  private final ShadowlistView mView;

  private boolean mStickyHeader = false;
  private boolean mStickyFooter = false;

  /*
   * Direction-based auto-hide: the header/footer pins to its edge and slides away as
   * the user scrolls toward the content, sliding back the other way. mHeaderHidden /
   * mFooterHidden track how far each is currently slid away (px); mLastAutoHideOffset
   * is the previous offset used to derive the scroll delta.
   */
  private boolean mAutoHideHeader = false;
  private boolean mAutoHideFooter = false;
  private float mHeaderHidden = 0f;
  private float mFooterHidden = 0f;
  private float mLastAutoHideOffset = 0f;

  /*
   * Sticky section headers (SectionList). The active section header is an
   * always-mounted overlay template (templateType "sectionHeader"); the core publishes
   * the resting geometry (offset + size in DIP along the scroll axis) of the in-flow
   * section headers through state, cached here, and we pin the overlay to the viewport
   * start on each scroll tick, pushing it up as the next in-flow header arrives.
   */
  private int[] mStickyHeaderIndices = new int[0];
  private double[] mStickyHeaderOffsets = new double[0];
  private double[] mStickyHeaderSizes = new double[0];

  ShadowlistStickyController(ShadowlistView view) {
    mView = view;
  }

  void setStickyHeader(boolean stickyHeader) {
    mStickyHeader = stickyHeader;
    applyStickyTranslations();
  }

  void setStickyFooter(boolean stickyFooter) {
    mStickyFooter = stickyFooter;
    applyStickyTranslations();
  }

  void setAutoHideHeader(boolean autoHideHeader) {
    mAutoHideHeader = autoHideHeader;
    applyStickyTranslations();
  }

  void setAutoHideFooter(boolean autoHideFooter) {
    mAutoHideFooter = autoHideFooter;
    applyStickyTranslations();
  }

  /*
   * Clear the auto-hide accumulators so a recycled host does not start with a stale
   * hide amount / scroll reference (iOS does this in prepareForRecycle).
   */
  void reset() {
    mHeaderHidden = 0f;
    mFooterHidden = 0f;
    mLastAutoHideOffset = 0f;
  }

  /*
   * Cache the sticky section-header geometry the core published this commit so the
   * per-scroll-tick pin (applyStickySectionHeaders) runs purely off cached values.
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
   * Pin the sticky header/footer to the viewport by translating them relative to
   * their resting position. The header tracks the scroll offset (stays at the
   * start); the footer tracks offset + window - content so it stays at the viewport
   * end and lands exactly on its resting position at the scroll extreme. Runs on
   * the UI thread per scroll frame, so it stays in lockstep with the gesture.
   */
  void applyStickyTranslations() {
    applyStickyTranslations(false);
  }

  /*
   * `accumulate` is true only for genuine user-scroll ticks, so the direction-based
   * auto-hide advances its hide/reveal amount on real scrolling but not on programmatic
   * offset corrections (MVCP, scrollToIndex, initial positioning), which would otherwise
   * slide it spuriously.
   */
  void applyStickyTranslations(boolean accumulate) {
    ViewGroup contentView = mView.getContentView();
    ViewGroup scrollView = mView.getScrollView();
    if (contentView == null || scrollView == null) {
      return;
    }
    boolean horizontal = mView.isHorizontal();

    int offsetX = scrollView.getScrollX();
    int offsetY = scrollView.getScrollY();
    int windowW = scrollView.getWidth();
    int windowH = scrollView.getHeight();
    int contentW = contentView.getWidth();
    int contentH = contentView.getHeight();

    float stickyLift = PixelUtil.toPixelFromDIP(STICKY_LIFT_DP);

    // Pre-scan the header/footer template sizes so each can be "pushed off" by the
    // other (the section-header swap motion) once they would collide.
    float headerSize = 0f;
    float footerSize = 0f;
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowlistTemplateView)) {
        continue;
      }
      String type = ((ShadowlistTemplateView) child).getTemplateType();
      if ("sectionHeader".equals(type)) {
        continue;
      }
      if ("footer".equals(type)) {
        footerSize = horizontal ? child.getWidth() : child.getHeight();
      } else {
        headerSize = horizontal ? child.getWidth() : child.getHeight();
      }
    }

    float axisOffset = horizontal ? offsetX : offsetY;
    float windowSize = horizontal ? windowW : windowH;
    float contentSize = horizontal ? contentW : contentH;

    // Direction-based auto-hide: accumulate the scroll delta into how far the header/
    // footer is slid away. Only advance on genuine user scrolls; a programmatic offset
    // jump snaps the reference without sliding the bar.
    float autoHideDelta = accumulate ? (axisOffset - mLastAutoHideOffset) : 0f;
    mLastAutoHideOffset = axisOffset;

    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (!(child instanceof ShadowlistTemplateView)) {
        continue;
      }
      String type = ((ShadowlistTemplateView) child).getTemplateType();
      // The section-header overlay is pinned (and Z-lifted) separately below.
      if ("sectionHeader".equals(type)) {
        continue;
      }

      boolean isFooter = "footer".equals(type);
      boolean autoHide = isFooter ? mAutoHideFooter : mAutoHideHeader;
      boolean sticky = isFooter ? mStickyFooter : mStickyHeader;

      float translation = 0f;
      if (isFooter) {
        if (mAutoHideFooter) {
          // Pin to the viewport end, slid down by mFooterHidden; shown near the bottom.
          float maxOffset = Math.max(0f, contentSize - windowSize);
          if (axisOffset >= maxOffset - footerSize) {
            mFooterHidden = 0f;
          } else {
            mFooterHidden = Math.max(0f, Math.min(mFooterHidden + autoHideDelta, footerSize));
          }
          translation = (axisOffset + windowSize - contentSize) + mFooterHidden;
        } else if (mStickyFooter) {
          // The footer always sticks to the viewport end (plain bottom pin).
          translation = axisOffset + windowSize - contentSize;
        }
      } else {
        if (mAutoHideHeader) {
          // Pin to the viewport start, slid up by mHeaderHidden; shown near the top.
          if (axisOffset <= headerSize) {
            mHeaderHidden = 0f;
          } else {
            mHeaderHidden = Math.max(0f, Math.min(mHeaderHidden + autoHideDelta, headerSize));
          }
          translation = axisOffset - mHeaderHidden;
        } else if (mStickyHeader) {
          // Pin to the viewport start, pushed back off by the content end (the footer's
          // resting top) as it scrolls up to meet it.
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
      // Lift a pinned / auto-hiding header/footer above the rows via Z; rest at 0.
      child.setTranslationZ((sticky || autoHide) ? stickyLift : 0f);
    }

    applyStickySectionHeaders();
  }

  /*
   * Pin the always-mounted section-header overlay to the viewport start, mirroring
   * Container::resolveStickyHeader. The overlay rests at the content origin and shows
   * the active section's header (its content is swapped in JS); here we only move it.
   * Walk the core-published in-flow header geometry for the active section (the last
   * header resting at/above the scroll offset) and the next one, pin the overlay to
   * the viewport start, and push it up as the next in-flow header scrolls in. When no
   * section is active (scrolled above the first header) the overlay is hidden, so the
   * regular list header shows through.
   */
  private void applyStickySectionHeaders() {
    ViewGroup contentView = mView.getContentView();
    ViewGroup scrollView = mView.getScrollView();
    if (contentView == null || scrollView == null) {
      return;
    }

    View overlay = findSectionHeaderOverlay();
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
     * Headers are ascending by offset: the active one is the last resting at/above
     * the viewport start, and the first one past it is the "next" that pushes it up.
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

    /*
     * The overlay rests at the content origin, so its translation along the scroll
     * axis is simply its displayed top: pinned to the viewport start (axisOffset),
     * pushed up to nextOffset - activeSize as the next in-flow header arrives.
     */
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
    // Keep the overlay above both the rows and a plain sticky header via Z (draw
    // order) instead of bringToFront(), which would reorder mContentView's children
    // and desync Fabric's index-based mounting.
    overlay.setTranslationZ(PixelUtil.toPixelFromDIP(SECTION_OVERLAY_LIFT_DP));
  }

  private @Nullable View findSectionHeaderOverlay() {
    ViewGroup contentView = mView.getContentView();
    if (contentView == null) {
      return null;
    }
    for (int i = 0; i < contentView.getChildCount(); i++) {
      View child = contentView.getChildAt(i);
      if (child instanceof ShadowlistTemplateView
          && "sectionHeader".equals(((ShadowlistTemplateView) child).getTemplateType())) {
        return child;
      }
    }
    return null;
  }
}
