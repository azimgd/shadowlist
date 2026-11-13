package com.shadowlist.utils

private val darkColors = ShadowListUtilsThemeColors(
  background = 0xFF000000.toInt(),
  elevated = 0xFF1C1C1E.toInt(),
  elevated2 = 0xFF2C2C2E.toInt(),
  groupedBackground = 0xFF000000.toInt(),
  groupedCell = 0xFF1C1C1E.toInt(),
  label = 0xFFFFFFFF.toInt(),
  secondaryLabel = 0x99EBEBF5.toInt(),
  tertiaryLabel = 0x4DEBEBF5,
  separator = 0x99545458.toInt(),
  fill = 0x3D767680,
  accent = 0xFF0A84FF.toInt(),
  accentSoft = 0x290A84FF,
  onAccent = 0xFFFFFFFF.toInt(),
  blue = 0xFF0A84FF.toInt(),
  green = 0xFF30D158.toInt(),
  orange = 0xFFFF9F0A.toInt(),
  red = 0xFFFF453B.toInt(),
  redSoft = 0x29FF453B,
  avatarPalette = ShadowListUtilsThemeColors.AVATAR_PALETTE,
)

internal val ShadowListUtilsThemeColors.Companion.DARK: ShadowListUtilsThemeColors get() = darkColors
