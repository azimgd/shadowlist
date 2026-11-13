package com.shadowlist.utils

/*
 * The colors of a theme as ARGB ints. Alpha values are part of the color.
 */
data class ShadowListUtilsThemeColors(
  val background: Int,
  val elevated: Int,
  val elevated2: Int,
  val groupedBackground: Int,
  val groupedCell: Int,
  val label: Int,
  val secondaryLabel: Int,
  val tertiaryLabel: Int,
  val separator: Int,
  val fill: Int,
  val accent: Int,
  val accentSoft: Int,
  val onAccent: Int,
  val blue: Int,
  val green: Int,
  val orange: Int,
  val red: Int,
  val redSoft: Int,
  val avatarPalette: List<Int>,
) {
  /*
   * The light and dark colors and the avatar palette are declared on it in the theme files.
   */
  companion object
}
