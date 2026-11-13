package com.shadowlist.utils

import android.content.res.Configuration
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.ceil

/*
 * The theme tokens: the values the JS themes hold, the system theme and overrides.
 */
@RunWith(AndroidJUnit4::class)
class ThemeTest {
  private fun rgb(hex: Int): Int = 0xFF000000.toInt() or hex

  private fun argb(alpha: Double, red: Int, green: Int, blue: Int): Int =
    (Math.round(alpha * 255).toInt() shl 24) or (red shl 16) or (green shl 8) or blue

  /*
   * A CSS color as the JS themes write it: #RRGGBB or rgba(r,g,b,a).
   */
  private fun color(css: String): Int {
    if (css.startsWith("#")) return rgb(css.drop(1).toInt(16))
    val numbers = css.substringAfter("(").substringBefore(")").split(",").map { it.trim().toDouble() }
    assertEquals(css, 4, numbers.size)
    return argb(numbers[3], numbers[0].toInt(), numbers[1].toInt(), numbers[2].toInt())
  }

  private val colorTokens: List<Pair<String, (ShadowListUtilsThemeColors) -> Int>> = listOf(
    "background" to { it.background }, "elevated" to { it.elevated }, "elevated2" to { it.elevated2 },
    "groupedBackground" to { it.groupedBackground }, "groupedCell" to { it.groupedCell }, "label" to { it.label },
    "secondaryLabel" to { it.secondaryLabel }, "tertiaryLabel" to { it.tertiaryLabel }, "separator" to { it.separator },
    "fill" to { it.fill }, "accent" to { it.accent }, "accentSoft" to { it.accentSoft }, "onAccent" to { it.onAccent },
    "blue" to { it.blue }, "green" to { it.green }, "orange" to { it.orange }, "red" to { it.red },
    "redSoft" to { it.redSoft },
  )

  private val styleTokens: List<Pair<String, (ShadowListUtilsThemeTypography) -> ShadowListUtilsTextStyle>> = listOf(
    "largeTitle" to { it.largeTitle }, "title2" to { it.title2 }, "title3" to { it.title3 },
    "headline" to { it.headline }, "body" to { it.body }, "callout" to { it.callout }, "subhead" to { it.subhead },
    "footnote" to { it.footnote }, "caption" to { it.caption },
  )

  private fun numbers(vector: JSONObject): Map<String, Float> =
    vector.keys().asSequence().associateWith { vector.getDouble(it).toFloat() }

  @Test
  fun the_light_colors_are_the_light_theme_of_the_templates() {
    val colors = ShadowListUtilsTheme.LIGHT.colors
    assertEquals(rgb(0xFFFFFF), colors.background)
    assertEquals(rgb(0xF2F2F7), colors.elevated)
    assertEquals(rgb(0xE5E5EA), colors.elevated2)
    assertEquals(rgb(0xF2F2F7), colors.groupedBackground)
    assertEquals(rgb(0xFFFFFF), colors.groupedCell)
    assertEquals(rgb(0x000000), colors.label)
    assertEquals(argb(0.6, 60, 60, 67), colors.secondaryLabel)
    assertEquals(argb(0.3, 60, 60, 67), colors.tertiaryLabel)
    assertEquals(argb(0.29, 60, 60, 67), colors.separator)
    assertEquals(argb(0.12, 118, 118, 128), colors.fill)
    assertEquals(rgb(0x007AFF), colors.accent)
    assertEquals(argb(0.16, 0, 122, 255), colors.accentSoft)
    assertEquals(rgb(0xFFFFFF), colors.onAccent)
    assertEquals(rgb(0x007AFF), colors.blue)
    assertEquals(rgb(0x34C759), colors.green)
    assertEquals(rgb(0xFF9500), colors.orange)
    assertEquals(rgb(0xFF3B30), colors.red)
    assertEquals(argb(0.16, 255, 59, 48), colors.redSoft)
    assertEquals(10, colors.avatarPalette.size)
    assertEquals(rgb(0xFF6B6B), colors.avatarPalette[0])
    assertEquals(rgb(0xC06C84), colors.avatarPalette[9])
  }

  @Test
  fun the_dark_colors_are_the_dark_theme_of_the_templates() {
    val colors = ShadowListUtilsTheme.DARK.colors
    assertEquals(rgb(0x000000), colors.background)
    assertEquals(rgb(0x1C1C1E), colors.elevated)
    assertEquals(rgb(0x2C2C2E), colors.elevated2)
    assertEquals(rgb(0x000000), colors.groupedBackground)
    assertEquals(rgb(0x1C1C1E), colors.groupedCell)
    assertEquals(rgb(0xFFFFFF), colors.label)
    assertEquals(argb(0.6, 235, 235, 245), colors.secondaryLabel)
    assertEquals(argb(0.3, 235, 235, 245), colors.tertiaryLabel)
    assertEquals(argb(0.6, 84, 84, 88), colors.separator)
    assertEquals(argb(0.24, 118, 118, 128), colors.fill)
    assertEquals(rgb(0x0A84FF), colors.accent)
    assertEquals(argb(0.16, 10, 132, 255), colors.accentSoft)
    assertEquals(rgb(0xFFFFFF), colors.onAccent)
    assertEquals(rgb(0x0A84FF), colors.blue)
    assertEquals(rgb(0x30D158), colors.green)
    assertEquals(rgb(0xFF9F0A), colors.orange)
    assertEquals(rgb(0xFF453B), colors.red)
    assertEquals(argb(0.16, 255, 69, 59), colors.redSoft)
  }

  @Test
  fun both_themes_share_the_avatar_palette() {
    val palette = listOf(
      0xFF6B6B, 0x4ECDC4, 0x45B7D1, 0xFFA07A, 0x98D8C8, 0xF7DC6F, 0xBB8FCE, 0x85C1E2, 0xF8B195, 0xC06C84).map(::rgb)
    assertEquals(ShadowListUtilsTheme.LIGHT.colors.avatarPalette, ShadowListUtilsTheme.DARK.colors.avatarPalette)
    assertEquals(palette, ShadowListUtilsTheme.LIGHT.colors.avatarPalette)
    assertEquals(10, ShadowListUtilsTheme.LIGHT.colors.avatarPalette.size)
  }

  @Test
  fun the_system_theme_follows_the_device_appearance() {
    fun themed(night: Int): ShadowListUtilsTheme {
      val configuration = Configuration(testContext.resources.configuration)
      configuration.uiMode = (configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK.inv()) or night
      return ShadowListUtilsTheme.system(testContext.createConfigurationContext(configuration))
    }
    assertSame(ShadowListUtilsTheme.LIGHT, themed(Configuration.UI_MODE_NIGHT_NO))
    assertSame(ShadowListUtilsTheme.DARK, themed(Configuration.UI_MODE_NIGHT_YES))
  }

  @Test
  fun the_text_styles_are_the_typography_of_the_templates() {
    val typography = ShadowListUtilsTheme.LIGHT.typography
    fun style(size: Float, weight: ShadowListUtilsFontWeight, lineHeight: Float, letterSpacing: Float) =
      ShadowListUtilsTextStyle(size, weight, lineHeight, letterSpacing)
    assertEquals(style(34f, ShadowListUtilsFontWeight.BOLD, 41f, 0.37f), typography.largeTitle)
    assertEquals(style(22f, ShadowListUtilsFontWeight.BOLD, 28f, 0.35f), typography.title2)
    assertEquals(style(20f, ShadowListUtilsFontWeight.SEMIBOLD, 25f, 0.38f), typography.title3)
    assertEquals(style(17f, ShadowListUtilsFontWeight.SEMIBOLD, 22f, -0.43f), typography.headline)
    assertEquals(style(17f, ShadowListUtilsFontWeight.REGULAR, 22f, -0.43f), typography.body)
    assertEquals(style(16f, ShadowListUtilsFontWeight.REGULAR, 21f, -0.31f), typography.callout)
    assertEquals(style(15f, ShadowListUtilsFontWeight.REGULAR, 20f, -0.24f), typography.subhead)
    assertEquals(style(13f, ShadowListUtilsFontWeight.REGULAR, 18f, -0.08f), typography.footnote)
    assertEquals(style(12f, ShadowListUtilsFontWeight.REGULAR, 16f, 0f), typography.caption)
  }

  @Test
  fun spacing_radius_and_the_grouped_tokens_are_the_base_tokens_of_the_templates() {
    val theme = ShadowListUtilsTheme.LIGHT
    assertEquals(ShadowListUtilsThemeSpacing(2f, 4f, 8f, 12f, 16f, 20f, 24f), theme.spacing)
    assertEquals(ShadowListUtilsThemeRadius(8f, 12f, 16f, 20f, 999f), theme.radius)
    assertEquals(ShadowListUtilsThemeGrouped(16f, 10f, 16f, 44f, ShadowListUtilsCaptionCase.UPPER, true), theme.grouped)
    assertEquals(68f, theme.rowInset)
    assertEquals(44f, theme.tapTarget)
    assertEquals(1, ShadowListUtilsTheme.HAIRLINE_PX)
  }

  @Test
  fun a_copied_theme_changes_one_token_and_keeps_the_rest() {
    val light = ShadowListUtilsTheme.LIGHT
    val theme = light.copy(
      colors = light.colors.copy(accent = 0xFFFF7A00.toInt()), spacing = light.spacing.copy(lg = 20f))
    assertEquals(0xFFFF7A00.toInt(), theme.colors.accent)
    assertNotEquals(light, theme)
    assertEquals(light.colors.label, theme.colors.label)
    assertEquals(light.typography, theme.typography)
    assertEquals(rgb(0x007AFF), ShadowListUtilsTheme.LIGHT.colors.accent)
    assertEquals(16f, ShadowListUtilsTheme.LIGHT.spacing.lg)
  }

  @Test
  fun a_text_style_typesets_lines_of_its_line_height() {
    val body = ShadowListUtilsTheme.LIGHT.typography.body
    val one = body.layout("One line", 300f.px)
    assertEquals(1, one.lineCount)
    assertEquals(body.scaledLineHeightPx(), one.height)
    val wrapped = body.layout("word ".repeat(40), 200f.px)
    assertTrue(wrapped.lineCount > 2)
    assertEquals(wrapped.lineCount * body.scaledLineHeightPx(), wrapped.height)
    assertEquals(2, body.layout("word ".repeat(40), 200f.px, 2).lineCount)
  }

  @Test
  fun a_weight_changes_only_the_weight_of_a_style() {
    val typography = ShadowListUtilsTheme.LIGHT.typography
    assertEquals(typography.headline, typography.body.with(ShadowListUtilsFontWeight.SEMIBOLD))
    assertEquals(
      ShadowListUtilsTextStyle(13f, ShadowListUtilsFontWeight.BOLD, 18f, -0.08f),
      typography.footnote.with(ShadowListUtilsFontWeight.BOLD))
    assertEquals(ShadowListUtilsFontWeight.REGULAR, typography.footnote.fontWeight)
  }

  @Test
  fun a_text_style_follows_the_readers_text_size() {
    val body = ShadowListUtilsTheme.LIGHT.typography.body
    val plain = withTextSize(1f) { body.layout("One line", 300f.px) }
    withTextSize(2f) {
      val large = body.layout("One line", 300f.px)
      assertEquals(ceil(44f.pxf).toInt(), large.height)
      assertEquals(ceil(44f.pxf).toInt(), body.scaledLineHeightPx())
      assertTrue(large.width > plain.width * 1.8)
      assertTrue(ShadowListUtilsPlatform.isLargeText)
      // A style that is not scaled keeps its size, like initials in an avatar.
      val unscaled = body.layout("One line", 300f.px, 1, scaled = false)
      assertEquals(plain.width, unscaled.width)
      assertEquals(plain.height, unscaled.height)
    }
    withTextSize(1.25f) {
      // A line height between pixels rounds up to the next one.
      assertEquals(ceil(27.5f.pxf).toInt(), body.scaledLineHeightPx())
      assertFalse(ShadowListUtilsPlatform.isLargeText)
    }
    // Without the override the style follows the device again.
    val device = body.layout("One line", 300f.px)
    assertEquals(body.scaledLineHeightPx(), device.height)
    assertEquals(ceil(body.lineHeight.spf).toInt(), device.height)
  }

  /*
   * theme.json holds the JS lightTheme and darkTheme as two cases, with input.scheme and the
   * theme as the output. A token changed in JS fails here until the native theme follows.
   */
  @Test
  fun the_themes_match_the_vectors() {
    val cases = Vectors.cases("theme.json")
    val themes = mapOf("light" to ShadowListUtilsTheme.LIGHT, "dark" to ShadowListUtilsTheme.DARK)
    assertEquals(setOf("light", "dark"), cases.map { it.getJSONObject("input").getString("scheme") }.toSet())
    for (entry in cases) {
      val name = entry.getJSONObject("input").getString("scheme")
      val theme = requireNotNull(themes[name]) { name }
      val vector = entry.getJSONObject("output")
      val colors = vector.getJSONObject("colors")
      for ((token, read) in colorTokens) {
        assertEquals("$name $token", color(colors.getString(token)), read(theme.colors))
      }
      val palette = colors.getJSONArray("avatarPalette")
      assertEquals(List(palette.length()) { color(palette.getString(it)) }, theme.colors.avatarPalette)
      val typography = vector.getJSONObject("typography")
      val weights = mapOf(
        "400" to ShadowListUtilsFontWeight.REGULAR, "600" to ShadowListUtilsFontWeight.SEMIBOLD,
        "700" to ShadowListUtilsFontWeight.BOLD)
      for ((token, read) in styleTokens) {
        val style = typography.getJSONObject(token)
        val expected = ShadowListUtilsTextStyle(
          fontSize = style.getDouble("fontSize").toFloat(),
          fontWeight = requireNotNull(weights[style.getString("fontWeight")]) { "$name $token" },
          lineHeight = style.getDouble("lineHeight").toFloat(),
          letterSpacing = style.getDouble("letterSpacing").toFloat())
        assertEquals("$name $token", expected, read(theme.typography))
      }
      val spacing = theme.spacing
      assertEquals(
        mapOf(
          "xxs" to spacing.xxs, "xs" to spacing.xs, "sm" to spacing.sm, "md" to spacing.md, "lg" to spacing.lg,
          "xl" to spacing.xl, "xxl" to spacing.xxl),
        numbers(vector.getJSONObject("spacing")))
      val radius = theme.radius
      assertEquals(
        mapOf("sm" to radius.sm, "md" to radius.md, "lg" to radius.lg, "xl" to radius.xl, "pill" to radius.pill),
        numbers(vector.getJSONObject("radius")))
      val grouped = vector.getJSONObject("grouped")
      assertEquals(grouped.getDouble("inset").toFloat(), theme.grouped.inset)
      assertEquals(grouped.getDouble("radius").toFloat(), theme.grouped.radius)
      assertEquals(grouped.getDouble("rowInset").toFloat(), theme.grouped.rowInset)
      assertEquals(grouped.getDouble("rowHeight").toFloat(), theme.grouped.rowHeight)
      val captionCase = if (theme.grouped.captionCase == ShadowListUtilsCaptionCase.UPPER) "upper" else "none"
      assertEquals(grouped.getString("captionCase"), captionCase)
      assertEquals(grouped.getBoolean("chevron"), theme.grouped.chevron)
      assertEquals(vector.getDouble("rowInset").toFloat(), theme.rowInset)
      assertEquals(vector.getDouble("tapTarget").toFloat(), theme.tapTarget)
    }
  }
}
