package com.shadowlist.utils

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.util.LruCache
import android.widget.FrameLayout
import com.shadowlist.kit.ShadowListKitTextLayout

/*
 * A colored circle with the initials of a name. The color is the one given or the palette color
 * of the name, the same on every platform. An image covers the initials when it loads. The
 * initials keep their size whatever the text size setting. The avatar is hidden from
 * accessibility: the row it sits in reads the name. size is in dp.
 */
class ShadowListUtilsAvatar(
  context: Context,
  val size: Float = DEFAULT_SIZE_DP,
  theme: ShadowListUtilsTheme,
) : FrameLayout(context) {
  var theme: ShadowListUtilsTheme = theme
    set(value) {
      if (field == value) return
      field = value
      show(name, imageUrl, color)
    }

  private val sizePx = size.px
  private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
  private val image = ShadowListUtilsImageView(context)
  private var name = ""
  private var imageUrl: String? = null
  private var color: Int? = null
  private var initials: ShadowListKitTextLayout? = null

  init {
    setWillNotDraw(false)
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
    image.cornerRadius = size / 2f
    image.placeholderColor = 0
    addView(image, LayoutParams(sizePx, sizePx))
  }

  /*
   * Show a person. An empty name shows the color alone.
   */
  fun show(name: String, imageUrl: String? = null, color: Int? = null) {
    this.name = name
    this.imageUrl = imageUrl
    this.color = color
    val palette = theme.colors.avatarPalette
    paint.color = color ?: paletteColor(name, palette) ?: theme.colors.fill
    initials = if (name.isEmpty()) null else initialsLayout(name, size)
    image.url = imageUrl
    image.visibility = if (imageUrl == null) GONE else VISIBLE
    invalidate()
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    super.onMeasure(
      MeasureSpec.makeMeasureSpec(sizePx, MeasureSpec.EXACTLY),
      MeasureSpec.makeMeasureSpec(sizePx, MeasureSpec.EXACTLY))
  }

  override fun onDraw(canvas: Canvas) {
    val radius = sizePx / 2f
    canvas.drawCircle(radius, radius, radius, paint)
    val text = initials ?: return
    text.layout.paint.color = theme.colors.label
    canvas.save()
    canvas.translate((sizePx - text.width) / 2f, (sizePx - text.height) / 2f)
    text.layout.draw(canvas)
    canvas.restore()
  }

  // region Initials

  internal companion object {
    private const val DEFAULT_SIZE_DP = 40f

    private const val CACHE_ENTRIES = 512

    /*
     * Initials by name and their layouts by text and size. Both come from a row's layout on any
     * thread, and LruCache locks itself.
     */
    private val initialsByName = LruCache<String, String>(CACHE_ENTRIES)
    private val layouts = LruCache<String, ShadowListKitTextLayout>(CACHE_ENTRIES)

    fun initials(name: String): String {
      initialsByName.get(name)?.let { return it }
      return ShadowListUtilsCore.avatarInitials(name).uppercase().also { initialsByName.put(name, it) }
    }

    /*
     * The initials of a name typeset for an avatar of a size, once per pair.
     */
    private fun initialsLayout(name: String, size: Float): ShadowListKitTextLayout {
      val text = initials(name)
      val key = "$text#$size"
      layouts.get(key)?.let { return it }
      val metrics = DoubleArray(ShadowListUtilsCore.AVATAR_OUT_SLOTS)
      ShadowListUtilsCore.avatarInitialsMetrics(size.toDouble(), metrics)
      val style = ShadowListUtilsTextStyle(
        metrics[ShadowListUtilsCore.AVATAR_OUT_FONT_SIZE].toFloat(), ShadowListUtilsFontWeight.SEMIBOLD,
        metrics[ShadowListUtilsCore.AVATAR_OUT_LINE_HEIGHT].toFloat(), 0f)
      return style.layout(text, size.px, 1, scaled = false).also { layouts.put(key, it) }
    }

    /*
     * The name's slot of the palette, the same on every platform, or null for an empty palette,
     * which the fill color stands in for.
     */
    fun paletteColor(name: String, palette: List<Int>): Int? {
      if (palette.isEmpty()) return null
      return palette.getOrNull(ShadowListUtilsCore.avatarColorIndex(name, palette.size))
    }
  }

  // endregion
}
