package com.shadowlist.utils

import android.content.Context
import android.content.res.Configuration

/*
 * The gaps between and around things.
 */
data class ShadowListUtilsThemeSpacing(
  val xxs: Float = 2f,
  val xs: Float = 4f,
  val sm: Float = 8f,
  val md: Float = 12f,
  val lg: Float = 16f,
  val xl: Float = 20f,
  val xxl: Float = 24f,
)

/*
 * The corner radii. pill rounds a corner all the way.
 */
data class ShadowListUtilsThemeRadius(
  val sm: Float = 8f,
  val md: Float = 12f,
  val lg: Float = 16f,
  val xl: Float = 20f,
  val pill: Float = 999f,
)

enum class ShadowListUtilsCaptionCase { UPPER, NONE }

/*
 * The numbers of grouped rows and their captions.
 */
data class ShadowListUtilsThemeGrouped(
  val inset: Float = 16f,
  val radius: Float = 10f,
  val rowInset: Float = 16f,
  val rowHeight: Float = 44f,
  val captionCase: ShadowListUtilsCaptionCase = ShadowListUtilsCaptionCase.UPPER,
  val chevron: Boolean = true,
)

/*
 * The design tokens the templates draw with: the same values as the light and dark themes of
 * shadowlist-utils. Lengths are dp. A theme is a value. Copy one to override it:
 *
 *   val theme = ShadowListUtilsTheme.LIGHT.let { it.copy(colors = it.colors.copy(accent = 0xFFFF7A00.toInt())) }
 *
 * A row holds the theme it was built with. There is no global current theme.
 * rowInset is where a row's text starts after its avatar, and where its separator starts.
 * tapTarget is the smallest side of something a finger presses.
 */
data class ShadowListUtilsTheme(
  val colors: ShadowListUtilsThemeColors,
  val typography: ShadowListUtilsThemeTypography = ShadowListUtilsThemeTypography(),
  val spacing: ShadowListUtilsThemeSpacing = ShadowListUtilsThemeSpacing(),
  val radius: ShadowListUtilsThemeRadius = ShadowListUtilsThemeRadius(),
  val grouped: ShadowListUtilsThemeGrouped = ShadowListUtilsThemeGrouped(),
  val rowInset: Float = 68f,
  val tapTarget: Float = 44f,
  val pressFeedback: ShadowListUtilsThemePressFeedback = ShadowListUtilsThemePressFeedback.HIGHLIGHT,
) {
  companion object {
    val LIGHT = ShadowListUtilsTheme(colors = ShadowListUtilsThemeColors.LIGHT)
    val DARK = ShadowListUtilsTheme(colors = ShadowListUtilsThemeColors.DARK)

    /*
     * Follows the device appearance: LIGHT in light mode and DARK in dark mode, for the
     * appearance the context has now.
     */
    fun system(context: Context): ShadowListUtilsTheme {
      val night = context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK
      return if (night == Configuration.UI_MODE_NIGHT_YES) DARK else LIGHT
    }

    /*
     * The thickness of a separator: one device pixel.
     */
    const val HAIRLINE_PX = 1
  }
}
