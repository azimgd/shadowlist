package com.shadowlist;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.uimanager.PixelUtil;

/*
 * Native sticky pinning for the header, footer and section-header overlay. Runs on the
 * UI thread every scroll tick off the core-published state cached here.
 */
class ShadowlistStickyController {
  // Z lift (draw order) for pinned views; never use bringToFront(), which reorders
  // the content child array and desyncs index-based child mounting.
  private static final int STICKY_LIFT_DP = 4;
  private static final int SECTION_OVERLAY_LIFT_DP = 6;

  private final ShadowlistView mView;

  private boolean mStickyHeader = false;
  private boolean mStickyFooter = false;

  // Direction-based auto-hide. mHeaderHidden/mFooterHidden = how far each is slid away
  // (px); mLastAutoHideOffset = previous offset, for the scroll delta.
  private boolean mAutoHideHeader = false;
  private boolean mAutoHideFooter = false;
  private float mHeaderHidden = 0f;
  private float mFooterHidden = 0f;
  private float mLastAutoHideOffset = 0f;

  // Sticky section headers: core-published resting geometry (offset + size in DIP along
  // the scroll axis) of the in-flow section headers, cached for the per-tick pin.
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

  // Clear the auto-hide accumulators so a recycled host has no stale state.
  void reset() {
    mHeaderHidden = 0f;
    mFooterHidden = 0f;
    mLastAutoHideOffset = 0f;
  }

  // Cache the core-published sticky section-header geometry for the per-tick pin.
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

  // Pin the sticky header/footer by translating them relative to their resting position.
  void applyStickyTranslations() {
    applyStickyTranslations(false);
  }

  // `accumulate` is true only for genuine user-scroll ticks; programmatic offset
  // corrections must not advance the auto-hide amount.
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

    // Pre-scan header/footer sizes so each can be pushed off by the other on collision.
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

    // Auto-hide scroll delta; only advances on genuine user scrolls.
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
        float footerStart = horizontal ? child.getLeft() : child.getTop();
        float restingTranslation = (axisOffset + windowSize - footerSize) - footerStart;
        if (mAutoHideFooter) {
          // Pin to the viewport end, slid down by mFooterHidden; shown near the bottom.
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
          // Pin to the viewport start, slid up by mHeaderHidden; shown near the top.
          if (axisOffset <= headerSize) {
            mHeaderHidden = 0f;
          } else {
            mHeaderHidden = Math.max(0f, Math.min(mHeaderHidden + autoHideDelta, headerSize));
          }
          translation = axisOffset - mHeaderHidden;
        } else if (mStickyHeader) {
          // Pin to the viewport start, pushed off by the content end on collision.
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
      // Lift a pinned/auto-hiding header/footer above the rows; rest at 0.
      child.setTranslationZ((sticky || autoHide) ? stickyLift : 0f);
    }

    applyStickySectionHeaders();
  }

  // Pin the section-header overlay to the viewport start, pushing it up as the next
  // in-flow header arrives. Hidden when no section is active.
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

    // Headers ascend by offset: active = last at/above the viewport start; next = first past it.
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

    // Displayed top: viewport start, pushed up to nextOffset - activeSize on arrival.
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
    // Keep the overlay above the rows and a plain sticky header via Z order.
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
