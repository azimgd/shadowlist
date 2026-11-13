package com.shadowlist.utils

import android.app.Presentation
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.Rect
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.ColorDrawable
import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.media.ImageReader
import android.os.Build
import android.text.format.DateFormat
import android.view.View
import android.view.View.MeasureSpec
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ProgressBar
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.shadowlist.kit.ShadowListKitTextView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.abs
import kotlin.math.floor
import kotlin.math.hypot
import kotlin.math.sqrt

/*
 * The primitives, the empty state, the time labels and the calls into the core they rest on.
 */
@RunWith(AndroidJUnit4::class)
class PrimitivesTest {
  private val theme = testTheme
  private val width = TEST_WIDTH_DP.px

  // region Fixtures

  /*
   * Lays a view out at the test width and the height it asks for, and returns that height.
   */
  private fun fit(view: View, widthPx: Int = width): Int {
    view.measure(MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
    view.layout(0, 0, view.measuredWidth, view.measuredHeight)
    return view.measuredHeight
  }

  /*
   * What a view draws, with a margin around its frame that a view drawing past its box reaches.
   */
  private fun drawn(view: View, margin: Int = 0): Bitmap {
    val bitmap = Bitmap.createBitmap(view.width + 2 * margin, view.height + 2 * margin, Bitmap.Config.ARGB_8888)
    val canvas = Canvas(bitmap)
    canvas.translate(margin.toFloat(), margin.toFloat())
    view.draw(canvas)
    return bitmap
  }

  /*
   * The pixel a bitmap holds where these colors were drawn over each other, first one first.
   */
  private fun pixel(vararg colors: Int): Int {
    val bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888)
    val canvas = Canvas(bitmap)
    for (color in colors) canvas.drawColor(color)
    return bitmap.getPixel(0, 0)
  }

  /*
   * The smallest frame around the pixels a test counts as ink, or null without any.
   */
  private fun inkBounds(bitmap: Bitmap, isInk: (x: Int, y: Int, color: Int) -> Boolean): Rect? {
    var bounds: Rect? = null
    for (y in 0 until bitmap.height) {
      for (x in 0 until bitmap.width) {
        if (!isInk(x, y, bitmap.getPixel(x, y))) continue
        bounds = (bounds ?: Rect(x, y, x + 1, y + 1)).apply { union(x, y, x + 1, y + 1) }
      }
    }
    return bounds
  }

  /*
   * The frame of what is drawn, without the faint pixels smoothing leaves around an edge.
   */
  private fun drawnBounds(bitmap: Bitmap): Rect =
    requireNotNull(inkBounds(bitmap) { _, _, color -> Color.alpha(color) >= 32 }) { "nothing is drawn" }

  /*
   * The color an avatar's circle is drawn in, read above its initials.
   */
  private fun circleColor(avatar: ShadowListUtilsAvatar): Int {
    fit(avatar)
    return drawn(avatar).getPixel(avatar.width / 2, avatar.height / 10)
  }

  /*
   * The frame of the initials an avatar draws: the pixels inside its circle that are not the
   * circle's color.
   */
  private fun initialsBounds(name: String): Rect {
    val avatar = ShadowListUtilsAvatar(testContext, theme = theme)
    avatar.show(name)
    val circle = circleColor(avatar)
    val radius = avatar.width / 2f
    val bounds = inkBounds(drawn(avatar)) { x, y, color ->
      color != circle && hypot(x + 0.5f - radius, y + 0.5f - radius) < radius * 0.9f
    }
    return requireNotNull(bounds) { "the avatar of $name draws no initials" }
  }

  private fun backgroundColor(view: View): Int = (view.background as ColorDrawable).color

  private class RecordingLoader : ShadowListUtilsImageLoader() {
    val loaded = ArrayList<String>()
    var cancelled = 0

    override fun load(url: String, into: ShadowListUtilsImageView) {
      loaded.add(url)
    }

    override fun cancel(view: ShadowListUtilsImageView) {
      ++cancelled
    }
  }

  /*
   * Waits until a condition holds on the UI thread, like for the frame that lays a window out.
   */
  private fun waitFor(what: String, condition: () -> Boolean) {
    val deadline = System.currentTimeMillis() + 5_000
    while (System.currentTimeMillis() < deadline) {
      var holds = false
      onMain { holds = condition() }
      if (holds) return
      Thread.sleep(16)
    }
    throw AssertionError("timed out waiting until $what")
  }

  // endregion

  // region Core

  @Test
  fun the_core_answers_the_avatar_vectors() {
    val cases = Vectors.cases("avatar.json")
    assertTrue(cases.isNotEmpty())
    for (entry in cases) {
      val name = entry.getString("name")
      val input = entry.getJSONObject("input")
      val output = entry.getJSONObject("output")
      // A case with half a surrogate pair lists its UTF-16 units, which not every JSON reader takes in a string.
      val units = input.optJSONArray("units")
      val text = if (units == null) input.getString("name") else String(CharArray(units.length()) { units.getInt(it).toChar() })
      assertEquals(name, output.getInt("colorIndex"), ShadowListUtilsCore.avatarColorIndex(text, input.getInt("paletteSize")))
      assertEquals(name, output.getString("initials"), ShadowListUtilsCore.avatarInitials(text))
    }
  }

  @Test
  fun the_core_reads_a_byte_order_mark_as_white_space() {
    assertEquals(1, ShadowListUtilsCore.avatarColorIndex("﻿Zoe Quinn", 8))
    assertEquals("ZQ", ShadowListUtilsCore.avatarInitials("﻿Zoe Quinn"))
  }

  @Test
  fun the_core_keeps_an_astral_character_whole() {
    assertEquals("😀𝓐", ShadowListUtilsCore.avatarInitials("😀mile 𝓐da"))
    assertEquals("𝓐", ShadowListUtilsCore.sectionInitial("𝓐da"))
    assertEquals("😀Q", ShadowListUtilsAvatar.initials("😀mile Quinn"))
  }

  @Test
  fun the_core_reads_half_a_surrogate_pair_as_the_replacement_character() {
    // A high surrogate with no low one after it, which no UTF-8 string holds. Every platform hashes U+FFFD in its place.
    val name = String(charArrayOf(0xD83D.toChar(), 0x41.toChar()))
    assertEquals((0xFFFD * 31 + 0x41) % 1000, ShadowListUtilsCore.avatarColorIndex(name, 1000))
    assertEquals("\uFFFD", ShadowListUtilsCore.avatarInitials(name))
  }

  @Test
  fun the_core_answers_the_relative_time_vectors() {
    val units = mapOf(
      "now" to ShadowListUtilsCore.RELATIVE_TIME_NOW, "minutes" to ShadowListUtilsCore.RELATIVE_TIME_MINUTES,
      "hours" to ShadowListUtilsCore.RELATIVE_TIME_HOURS, "days" to ShadowListUtilsCore.RELATIVE_TIME_DAYS,
      "date" to ShadowListUtilsCore.RELATIVE_TIME_DATE)
    val cases = Vectors.cases("relative-time.json")
    assertTrue(cases.isNotEmpty())
    for (entry in cases) {
      val name = entry.getString("name")
      val input = entry.getJSONObject("input")
      val output = entry.getJSONObject("output")
      val time = ShadowListUtilsRelativeTime(
        ShadowListUtilsCore.relativeTime(input.getDouble("dateMs"), input.getDouble("nowMs")))
      assertEquals(name, units[output.getString("unit")], time.unit)
      assertEquals(name, output.getInt("count"), time.count)
    }
  }

  @Test
  fun the_core_frames_a_feed_image() {
    fun frame(width: Double, height: Double): DoubleArray =
      DoubleArray(ShadowListUtilsCore.FEED_OUT_SLOTS).also { ShadowListUtilsCore.feedSingleImageFrame(width, height, it) }
    assertEquals(1.0, frame(1000.0, 1000.0)[ShadowListUtilsCore.FEED_OUT_ASPECT_RATIO], 0.0)
    assertEquals(0.75, frame(600.0, 1600.0)[ShadowListUtilsCore.FEED_OUT_ASPECT_RATIO], 0.0)
    assertEquals(16.0 / 9.0, frame(3000.0, 500.0)[ShadowListUtilsCore.FEED_OUT_ASPECT_RATIO], 1e-9)
    val unsized = frame(0.0, 0.0)
    assertEquals(0.0, unsized[ShadowListUtilsCore.FEED_OUT_ASPECT_RATIO], 0.0)
    assertEquals(200.0, unsized[ShadowListUtilsCore.FEED_OUT_HEIGHT], 0.0)
  }

  @Test
  fun the_core_gives_the_layout_constants() {
    assertEquals(14f, ShadowListUtilsCore.CHAT_BUBBLE_PADDING_HORIZONTAL_DP)
    assertEquals(0.75f, ShadowListUtilsCore.CHAT_BUBBLE_WIDTH_FRACTION)
    assertEquals(200f, ShadowListUtilsCore.FEED_SINGLE_IMAGE_HEIGHT_DP)
    assertEquals("#", ShadowListUtilsCore.SECTION_OTHER_TITLE)
    val metrics = DoubleArray(ShadowListUtilsCore.AVATAR_OUT_SLOTS)
    ShadowListUtilsCore.avatarInitialsMetrics(40.0, metrics)
    assertEquals(17.0, metrics[ShadowListUtilsCore.AVATAR_OUT_FONT_SIZE], 0.0)
    assertEquals(21.0, metrics[ShadowListUtilsCore.AVATAR_OUT_LINE_HEIGHT], 0.0)
    assertTrue(ShadowListUtilsCore.relativeTimeHasDate(0.0))
    assertFalse(ShadowListUtilsCore.relativeTimeHasDate(Double.NaN))
  }

  // endregion

  // region Labels

  @Test
  fun relative_time_labels_word_each_bucket() {
    val labels = ShadowListUtilsRelativeTimeLabels()
    val now = 1_762_905_600_000.0
    assertEquals("now", labels.format(now - 5_000, now))
    assertEquals("5m", labels.format(now - 5 * 60_000, now))
    assertEquals("3h", labels.format(now - 3 * 3_600_000, now))
    assertEquals("2d", labels.format(now - 2 * 86_400_000.0, now))
    val locale = Locale.getDefault()
    val formatter = SimpleDateFormat(DateFormat.getBestDateTimePattern(locale, "MMMd"), locale)
    val old = now - 30 * 86_400_000.0
    assertEquals(formatter.format(Date(old.toLong())), labels.format(old, now))
  }

  @Test
  fun relative_time_labels_take_the_apps_words() {
    val labels = ShadowListUtilsRelativeTimeLabels().copy(now = "just now", minutes = { "$it min" })
    val now = 1_762_905_600_000.0
    assertEquals("just now", labels.format(now, now))
    assertEquals("2 min", labels.format(now - 120_000, now))
  }

  @Test
  fun relative_time_labels_write_nothing_for_a_date_that_is_not_finite() {
    val labels = ShadowListUtilsRelativeTimeLabels()
    val now = 1_762_905_600_000.0
    assertEquals("", labels.format(Double.NaN, now))
    assertEquals("", labels.format(Double.NEGATIVE_INFINITY, now))
    assertFalse(labels.format(now - 30 * 86_400_000.0, now).isEmpty())
  }

  // endregion

  // region Views

  @Test
  fun an_avatar_is_a_circle_of_its_size_in_the_palette_color_of_the_name() = onMain {
    val avatar = ShadowListUtilsAvatar(testContext, theme = theme)
    avatar.show("Ada Lovelace")
    assertEquals(40f, avatar.size)
    fit(avatar)
    assertEquals(40f.px, avatar.measuredWidth)
    assertEquals(40f.px, avatar.measuredHeight)
    val palette = theme.colors.avatarPalette
    val index = ShadowListUtilsCore.avatarColorIndex("Ada Lovelace", palette.size)
    assertEquals(palette[index], ShadowListUtilsAvatar.paletteColor("Ada Lovelace", palette))
    assertEquals(pixel(palette[index]), circleColor(avatar))
    // A circle leaves the corners of its square empty.
    val bitmap = drawn(avatar)
    assertEquals(0, bitmap.getPixel(0, 0))
    assertEquals(0, bitmap.getPixel(bitmap.width - 1, bitmap.height - 1))
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS, avatar.importantForAccessibility)
    avatar.show("Ada Lovelace", color = Color.MAGENTA)
    assertEquals(pixel(Color.MAGENTA), circleColor(avatar))
    val small = ShadowListUtilsAvatar(testContext, 30f, theme)
    fit(small)
    assertEquals(30f.px, small.measuredWidth)
    assertEquals(30f.px, small.measuredHeight)
  }

  @Test
  fun an_avatar_without_a_palette_takes_the_fill_color() = onMain {
    val bare = theme.copy(colors = theme.colors.copy(avatarPalette = emptyList()))
    assertNull(ShadowListUtilsAvatar.paletteColor("Ada Lovelace", emptyList()))
    val avatar = ShadowListUtilsAvatar(testContext, theme = bare)
    avatar.show("Ada Lovelace")
    assertEquals(pixel(bare.colors.fill), circleColor(avatar))
  }

  @Test
  fun an_avatar_follows_a_new_theme() = onMain {
    val avatar = ShadowListUtilsAvatar(testContext, theme = ShadowListUtilsTheme.LIGHT)
    avatar.show("Ada Lovelace")
    val dark = ShadowListUtilsTheme.DARK
    val shifted = dark.copy(colors = dark.colors.copy(avatarPalette = dark.colors.avatarPalette.reversed()))
    avatar.theme = shifted
    val index = ShadowListUtilsCore.avatarColorIndex("Ada Lovelace", shifted.colors.avatarPalette.size)
    assertEquals(pixel(shifted.colors.avatarPalette[index]), circleColor(avatar))
    // The initials are drawn in the new theme's label color.
    val label = pixel(shifted.colors.label)
    assertNotEquals(pixel(ShadowListUtilsTheme.LIGHT.colors.label), label)
    assertTrue(inkBounds(drawn(avatar)) { _, _, color -> color == label } != null)
  }

  @Test
  fun an_avatars_initials_keep_their_size_at_a_large_text_size() = onMain {
    val plain = withTextSize(1f) { initialsBounds("Grace Hopper") }
    assertTrue(plain.height() > 0)
    // Another name with the same letters, which the avatars have not typeset before.
    val large = withTextSize(2f) { initialsBounds("Hedy Gamarr") }
    assertEquals(plain.height(), large.height())
    assertEquals(plain.width().toFloat(), large.width().toFloat(), 2f)
  }

  @Test
  fun an_image_view_fills_clips_and_shows_its_placeholder_color() = onMain {
    val view = ShadowListUtilsImageView(testContext)
    view.cornerRadius = 12f
    view.placeholderColor = theme.colors.elevated2
    assertTrue(view.clipToOutline)
    assertEquals(theme.colors.elevated2, backgroundColor(view))
    view.placeholderColor = ShadowListUtilsTheme.DARK.colors.elevated2
    assertEquals(ShadowListUtilsTheme.DARK.colors.elevated2, backgroundColor(view))
    // An image twice as wide as the view covers it around its center.
    host(view, 100, 100)
    val image = BitmapDrawable(view.resources, Bitmap.createBitmap(200, 100, Bitmap.Config.ARGB_8888))
    view.image = image
    assertEquals(Rect(-50, 0, 150, 100), image.bounds)
  }

  @Test
  fun an_image_view_asks_the_shared_loader_once_per_url_when_it_has_a_size() {
    val previous = ShadowListUtilsImageLoader.shared
    val recorder = RecordingLoader()
    ShadowListUtilsImageLoader.shared = recorder
    var display: VirtualDisplay? = null
    var reader: ImageReader? = null
    var presentation: Presentation? = null
    try {
      lateinit var view: ShadowListUtilsImageView
      // A view asks for its image in a window, which here sits on a display of the test's own.
      onMain {
        val metrics = testContext.resources.displayMetrics
        val surface = ImageReader.newInstance(400, 400, PixelFormat.RGBA_8888, 2).also { reader = it }.surface
        val manager = testContext.getSystemService(DisplayManager::class.java)
        val made = manager.createVirtualDisplay("ShadowListUtilsTest", 400, 400, metrics.densityDpi, surface, 0)
        display = made
        val container = FrameLayout(testContext)
        view = ShadowListUtilsImageView(testContext)
        container.addView(view, FrameLayout.LayoutParams(0, 0))
        presentation = Presentation(testContext, made.display).apply {
          setContentView(container)
          show()
        }
      }
      waitFor("the view is in a window") { view.isAttachedToWindow }
      onMain {
        view.url = "https://example.com/a.jpg"
        assertEquals(emptyList<String>(), recorder.loaded)
        view.layoutParams = FrameLayout.LayoutParams(100, 80)
      }
      waitFor("the view has its size") { view.width == 100 && view.height == 80 }
      onMain {
        assertEquals(listOf("https://example.com/a.jpg"), recorder.loaded)
        view.layoutParams = FrameLayout.LayoutParams(100, 90)
      }
      waitFor("the view has its new size") { view.height == 90 }
      onMain {
        assertEquals(listOf("https://example.com/a.jpg"), recorder.loaded)
        view.url = "https://example.com/b.jpg"
        assertEquals(listOf("https://example.com/a.jpg", "https://example.com/b.jpg"), recorder.loaded)
        // The view stops waiting for the url it had.
        assertEquals(1, recorder.cancelled)
        view.url = null
        assertEquals(2, recorder.cancelled)
        assertNull(view.image)
      }
    } finally {
      onMain { presentation?.dismiss() }
      display?.release()
      reader?.close()
      ShadowListUtilsImageLoader.shared = previous
    }
  }

  @Test
  fun an_item_separator_is_a_hairline_after_the_row_inset() = onMain {
    val separator = ShadowListUtilsItemSeparator(testContext, theme)
    assertEquals(68f, separator.inset)
    assertEquals(ShadowListUtilsTheme.HAIRLINE_PX, fit(separator))
    assertEquals(width, separator.measuredWidth)
    // The line starts at the inset, on the theme's background.
    val bitmap = drawn(separator)
    val inset = 68f.px
    assertEquals(pixel(theme.colors.background), bitmap.getPixel(inset - 2, 0))
    assertEquals(pixel(theme.colors.background, theme.colors.separator), bitmap.getPixel(inset + 2, 0))
    assertEquals(pixel(theme.colors.background, theme.colors.separator), bitmap.getPixel(width - 1, 0))
    assertEquals(theme.colors.background, backgroundColor(separator))
    separator.theme = ShadowListUtilsTheme.DARK
    assertEquals(ShadowListUtilsTheme.DARK.colors.background, backgroundColor(separator))
  }

  @Test
  fun a_list_header_is_its_title_and_subtitle_with_their_padding() = onMain {
    val typography = theme.typography
    val titled = ShadowListUtilsListHeader(testContext, "Lisbon Crew", theme = theme)
    assertEquals(4f.px + typography.largeTitle.scaledLineHeightPx() + 12f.px, fit(titled))
    val header = ShadowListUtilsListHeader(testContext, "Lisbon Crew", "8 travellers", theme)
    assertEquals(
      4f.px + typography.largeTitle.scaledLineHeightPx() + 2f.px + typography.subhead.scaledLineHeightPx() + 12f.px, fit(header))
    assertEquals(theme.colors.background, backgroundColor(header))
    val title = header.getChildAt(0) as ShadowListKitTextView
    val subtitle = header.getChildAt(1) as ShadowListKitTextView
    assertEquals(16f.px, title.left)
    assertEquals(4f.px, title.top)
    assertEquals(16f.px, subtitle.left)
    assertEquals(4f.px + typography.largeTitle.scaledLineHeightPx() + 2f.px, subtitle.top)
    assertEquals(theme.colors.label, title.textColor)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) assertTrue(title.isAccessibilityHeading)
  }

  @Test
  fun a_list_footer_centers_its_text_with_its_padding() = onMain {
    val footer = ShadowListUtilsListFooter(testContext, "42 travellers", theme)
    assertEquals(20f.px + theme.typography.footnote.scaledLineHeightPx() + 20f.px, fit(footer))
    val text = footer.getChildAt(0) as ShadowListKitTextView
    assertEquals(width / 2f, (text.left + text.right) / 2f, 1f)
    assertEquals(20f.px, text.top)
    assertEquals(theme.colors.secondaryLabel, text.textColor)
    // Every line centers itself in the text's width.
    val layout = requireNotNull(text.textLayout) { "the footer has no text" }.layout
    assertEquals(text.width / 2f, (layout.getLineLeft(0) + layout.getLineRight(0)) / 2f, 1f)
  }

  @Test
  fun a_section_header_is_one_line_between_its_padding() = onMain {
    val footnote = theme.typography.footnote
    val header = ShadowListUtilsSectionHeader("a", "a", 12, theme)
    val layout = header.layout(width) as ShadowListUtilsSectionHeaderLayout
    assertEquals(10f.px + footnote.scaledLineHeightPx() + 10f.px, layout.heightPx)
    assertLines(layout.title, footnote, 1)
    assertEquals(16f.px, layout.titleFrame.left)
    assertEquals(10f.px, layout.titleFrame.top)
    assertEquals(width - 16f.px, layout.countFrame.right)
    assertEquals(10f.px, layout.countFrame.top)
    // The title is in upper case unless the theme says otherwise.
    assertEquals("A", layout.title.layout.text.toString())
    assertEquals("12", layout.count?.layout?.text?.toString())
    val plain = theme.copy(grouped = theme.grouped.copy(captionCase = ShadowListUtilsCaptionCase.NONE))
    val kept = ShadowListUtilsSectionHeader("b", "b", theme = plain).layout(width) as ShadowListUtilsSectionHeaderLayout
    assertEquals("b", kept.title.layout.text.toString())
    assertNull(kept.count)
    assertTrue(kept.countFrame.isEmpty)

    val view = ShadowListUtilsSectionHeaderView(testContext)
    view.configure(header, layout)
    assertEquals(theme.colors.elevated, backgroundColor(view))
    assertEquals("a, 12", view.contentDescription)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) assertTrue(view.createAccessibilityNodeInfo().isHeading)
    assertNotEquals(header.contentVersion, ShadowListUtilsSectionHeader("a", "a", 13, theme).contentVersion)
  }

  @Test
  fun a_right_to_left_section_header_mirrors_its_frames() {
    val header = ShadowListUtilsSectionHeader("a", "a", 12, theme)
    val plain = header.layout(width) as ShadowListUtilsSectionHeaderLayout
    val mirrored = withRightToLeft { header.layout(width) } as ShadowListUtilsSectionHeaderLayout
    assertEquals(width - 16f.px, mirrored.titleFrame.right)
    assertEquals(16f.px, mirrored.countFrame.left)
    assertEquals(width - plain.titleFrame.right, mirrored.titleFrame.left)
    assertEquals(plain.titleFrame.width(), mirrored.titleFrame.width())
    assertEquals(plain.titleFrame.height(), mirrored.titleFrame.height())
  }

  @Test
  fun a_spinner_pads_the_system_indicator_and_reads_as_loading() = onMain {
    val spinner = ShadowListUtilsSpinner(testContext, theme = theme)
    assertEquals(20f.px + 2 * 16f.px, fit(spinner))
    assertEquals(width, spinner.measuredWidth)
    val indicator = spinner.getChildAt(0) as ProgressBar
    assertEquals(20f.px, indicator.width)
    assertEquals(20f.px, indicator.height)
    assertEquals((width - 20f.px) / 2, indicator.left)
    assertEquals(16f.px, indicator.top)
    assertEquals(theme.colors.secondaryLabel, indicator.indeterminateTintList?.defaultColor)
    assertEquals("Loading", spinner.contentDescription)
    assertTrue(indicator.isIndeterminate)
    val red = ShadowListUtilsSpinner(testContext, color = Color.RED, theme = theme).getChildAt(0) as ProgressBar
    assertEquals(Color.RED, red.indeterminateTintList?.defaultColor)
  }

  @Test
  fun redacted_text_is_one_centered_bar_per_line_that_holds_text() {
    val subhead = theme.typography.subhead
    val lineHeight = subhead.scaledLineHeightPx()
    val layout = subhead.layout("One long line of words that wraps.\n\nShort", 200f.px)
    assertTrue(layout.lineCount >= 4)
    val bars = ShadowListUtilsRedactedText.bars(layout, lineHeight)
    // The blank line has no bar.
    assertEquals(layout.lineCount - 1, bars.size)
    for (bar in bars) {
      val line = floor(bar.centerY() / lineHeight).toInt()
      assertEquals(lineHeight * 0.62f, bar.height(), 0.01f)
      assertEquals((line + 0.5f) * lineHeight, bar.centerY(), 0.01f)
      assertEquals(requireNotNull(layout.rectForLine(line)) { "no line $line" }.width(), bar.width(), 0.01f)
    }
    // A bar is as wide as its own line's text, not as the widest line.
    assertTrue(bars.last().width() < bars[0].width() / 2f)
    assertEquals(bars.map { it.top }.sorted(), bars.map { it.top })
  }

  @Test
  fun an_empty_state_stacks_its_icon_title_and_message_in_a_centered_column() = onMain {
    val typography = theme.typography
    val icon = View(testContext)
    icon.layoutParams = ViewGroup.LayoutParams(48f.px, 48f.px)
    val state = ShadowListUtilsEmptyState(testContext, "No messages", "New mail shows here.", icon, theme)
    assertEquals(
      28f.px + 48f.px + 4f.px + 6f.px + typography.body.scaledLineHeightPx() + 4f.px + typography.footnote.scaledLineHeightPx() + 28f.px,
      fit(state))
    assertEquals(Rect((width - 48f.px) / 2, 28f.px, (width + 48f.px) / 2, 28f.px + 48f.px), Rect(icon.left, icon.top, icon.right, icon.bottom))
    val texts = (0 until state.childCount).map(state::getChildAt).filterIsInstance<ShadowListKitTextView>()
    assertEquals(28f.px + 48f.px + 4f.px + 6f.px, texts[0].top)
    assertEquals(texts[0].bottom + 4f.px, texts[1].top)
    assertEquals(width / 2f, (texts[0].left + texts[0].right) / 2f, 1f)
    // The column stays at the top of a frame taller than it needs.
    host(state)
    assertEquals(28f.px, icon.top)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_YES, state.importantForAccessibility)
    assertEquals("No messages. New mail shows here.", state.contentDescription)
    val titled = ShadowListUtilsEmptyState(testContext, "No messages", theme = theme)
    assertEquals(28f.px + 6f.px + typography.body.scaledLineHeightPx() + 28f.px, fit(titled))
    assertEquals("No messages", titled.contentDescription)
  }

  @Test
  fun the_icons_are_drawn_inside_their_box() = onMain {
    val margin = 8
    fun ink(kind: ShadowListUtilsIconView.Kind, size: Int): Rect {
      val view = ShadowListUtilsIconView(testContext, kind)
      view.color = Color.BLACK
      host(view, size, size)
      return drawnBounds(drawn(view, margin)).apply { offset(-margin, -margin) }
    }
    val size = 20f.px
    for (kind in ShadowListUtilsIconView.Kind.values()) {
      val bounds = ink(kind, size)
      assertTrue("$kind draws at $bounds", Rect(0, 0, size, size).contains(bounds))
    }
    // Three lines as wide as the icon, 1.75 thick and 22 percent of the size apart, centered.
    val grip = ink(ShadowListUtilsIconView.Kind.GRIP, size)
    assertEquals(size, grip.width())
    assertEquals(3 * 1.75f.pxf + 2 * size * 0.22f, grip.height().toFloat(), 2f)
    assertEquals(size / 2f, grip.exactCenterY(), 1f)
    // The chevron's arms are 42 percent of the size and 2 thick, and its tip points right.
    val chevron = ink(ShadowListUtilsIconView.Kind.CHEVRON, size)
    val reach = size * 0.42f * sqrt(0.5f)
    assertEquals(2 * reach + 2f.pxf * sqrt(0.5f), chevron.height().toFloat(), 2f)
    assertEquals(size / 2f, chevron.exactCenterY(), 1f)
    assertTrue(tipIsAtTheRight(ShadowListUtilsIconView.Kind.CHEVRON, size))
    assertFalse(withRightToLeft { tipIsAtTheRight(ShadowListUtilsIconView.Kind.CHEVRON, size) })
    // The check's arms are 26 and 50 percent of its size and 2.2 thick.
    val checkSize = 14f.px
    val check = ink(ShadowListUtilsIconView.Kind.CHECK, checkSize)
    assertEquals(checkSize * (0.26f + 0.5f) * sqrt(0.5f) + 2.2f.pxf * sqrt(0.5f), check.width().toFloat(), 2f)
  }

  /*
   * Whether the row through an icon's middle reaches further right than left, like a chevron
   * that points right.
   */
  private fun tipIsAtTheRight(kind: ShadowListUtilsIconView.Kind, size: Int): Boolean {
    val view = ShadowListUtilsIconView(testContext, kind)
    view.color = Color.BLACK
    host(view, size, size)
    val bitmap = drawn(view)
    val all = drawnBounds(bitmap)
    val middle = requireNotNull(inkBounds(bitmap) { _, y, color -> y == size / 2 && Color.alpha(color) != 0 }) {
      "nothing is drawn through the middle"
    }
    assertTrue(abs((all.right - middle.right) - (middle.left - all.left)) > size / 8)
    return all.right - middle.right < middle.left - all.left
  }

  // endregion
}
