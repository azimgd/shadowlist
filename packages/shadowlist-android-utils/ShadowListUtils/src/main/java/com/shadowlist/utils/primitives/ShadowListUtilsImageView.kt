package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.drawable.Drawable
import android.view.View
import kotlin.math.max
import kotlin.math.roundToInt

/*
 * Shows a remote image, aspect filled and clipped, over a placeholder color. Setting url loads
 * it through ShadowListUtilsImageLoader.shared at the size the view is laid out at. A view that
 * gets a new url before the previous one arrives drops the previous result.
 */
class ShadowListUtilsImageView(context: Context) : View(context) {
  var url: String? = null
    set(value) {
      if (field == value) return
      if (field != null) ShadowListUtilsImageLoader.shared.cancel(this)
      field = value
      image = null
      requested = false
      request()
    }

  /*
   * The corner radius in dp.
   */
  var cornerRadius: Float = 0f
    set(value) {
      field = value
      outline.radius = value.pxf
      clipToOutline = value > 0f
      invalidateOutline()
    }

  /*
   * Shown behind the image and while it loads. Templates pass the theme's elevated2.
   */
  var placeholderColor: Int = ShadowListUtilsTheme.LIGHT.colors.elevated2
    set(value) {
      field = value
      setBackgroundColor(value)
    }

  /*
   * The image shown. An image loader sets it for the url it was asked to load.
   */
  var image: Drawable? = null
    set(value) {
      if (field === value) return
      field = value
      fitImage()
      invalidate()
    }

  private val outline = ShadowListUtilsRoundedOutline(0f)
  private var requested = false

  init {
    outlineProvider = outline
    setBackgroundColor(placeholderColor)
  }

  override fun onSizeChanged(width: Int, height: Int, previousWidth: Int, previousHeight: Int) {
    super.onSizeChanged(width, height, previousWidth, previousHeight)
    fitImage()
    request()
  }

  override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    request()
  }

  /*
   * A view that left its window stops waiting. It asks again when it comes back without an image.
   */
  override fun onDetachedFromWindow() {
    super.onDetachedFromWindow()
    if (url != null && image == null) {
      ShadowListUtilsImageLoader.shared.cancel(this)
      requested = false
    }
  }

  /*
   * Ask the loader once per url, as soon as the view has a size to decode for.
   */
  private fun request() {
    val url = url ?: return
    if (requested || width <= 0 || height <= 0 || !isAttachedToWindow) return
    requested = true
    ShadowListUtilsImageLoader.shared.load(url, this)
  }

  /*
   * Scale the image to cover the view around its center.
   */
  private fun fitImage() {
    val image = image ?: return
    val imageWidth = image.intrinsicWidth
    val imageHeight = image.intrinsicHeight
    if (imageWidth <= 0 || imageHeight <= 0 || width <= 0 || height <= 0) {
      image.setBounds(0, 0, width, height)
      return
    }
    val scale = max(width / imageWidth.toFloat(), height / imageHeight.toFloat())
    val scaledWidth = (imageWidth * scale).roundToInt()
    val scaledHeight = (imageHeight * scale).roundToInt()
    val left = (width - scaledWidth) / 2
    val top = (height - scaledHeight) / 2
    image.setBounds(left, top, left + scaledWidth, top + scaledHeight)
  }

  override fun onDraw(canvas: Canvas) {
    image?.draw(canvas)
  }
}
