package com.shadowlist.utils

/*
 * The text styles of a theme.
 */
data class ShadowListUtilsThemeTypography(
  val largeTitle: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(34f, ShadowListUtilsFontWeight.BOLD, 41f, 0.37f),
  val title2: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(22f, ShadowListUtilsFontWeight.BOLD, 28f, 0.35f),
  val title3: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(20f, ShadowListUtilsFontWeight.SEMIBOLD, 25f, 0.38f),
  val headline: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(17f, ShadowListUtilsFontWeight.SEMIBOLD, 22f, -0.43f),
  val body: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(17f, ShadowListUtilsFontWeight.REGULAR, 22f, -0.43f),
  val callout: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(16f, ShadowListUtilsFontWeight.REGULAR, 21f, -0.31f),
  val subhead: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(15f, ShadowListUtilsFontWeight.REGULAR, 20f, -0.24f),
  val footnote: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(13f, ShadowListUtilsFontWeight.REGULAR, 18f, -0.08f),
  val caption: ShadowListUtilsTextStyle = ShadowListUtilsTextStyle(12f, ShadowListUtilsFontWeight.REGULAR, 16f, 0f),
)
