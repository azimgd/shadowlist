package com.shadowlist.utils

private val avatarPalette: List<Int> = listOf(
  0xFFFF6B6B.toInt(), 0xFF4ECDC4.toInt(), 0xFF45B7D1.toInt(), 0xFFFFA07A.toInt(), 0xFF98D8C8.toInt(),
  0xFFF7DC6F.toInt(), 0xFFBB8FCE.toInt(), 0xFF85C1E2.toInt(), 0xFFF8B195.toInt(), 0xFFC06C84.toInt(),
)

private val lightColors = ShadowListUtilsThemeColors(
  background = 0xFFFFFFFF.toInt(),
  elevated = 0xFFF2F2F7.toInt(),
  elevated2 = 0xFFE5E5EA.toInt(),
  groupedBackground = 0xFFF2F2F7.toInt(),
  groupedCell = 0xFFFFFFFF.toInt(),
  label = 0xFF000000.toInt(),
  secondaryLabel = 0x993C3C43.toInt(),
  tertiaryLabel = 0x4D3C3C43,
  separator = 0x4A3C3C43,
  fill = 0x1F767680,
  accent = 0xFF007AFF.toInt(),
  accentSoft = 0x29007AFF,
  onAccent = 0xFFFFFFFF.toInt(),
  blue = 0xFF007AFF.toInt(),
  green = 0xFF34C759.toInt(),
  orange = 0xFFFF9500.toInt(),
  red = 0xFFFF3B30.toInt(),
  redSoft = 0x29FF3B30,
  avatarPalette = avatarPalette,
)

/*
 * The palette avatars and tiles pick from by name, the same in both themes.
 */
internal val ShadowListUtilsThemeColors.Companion.AVATAR_PALETTE: List<Int> get() = avatarPalette

internal val ShadowListUtilsThemeColors.Companion.LIGHT: ShadowListUtilsThemeColors get() = lightColors
