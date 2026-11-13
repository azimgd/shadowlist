package com.shadowlist.utils

import android.content.res.ColorStateList
import android.graphics.drawable.Drawable
import android.graphics.drawable.RippleDrawable

/* Native state/drawables own feedback; no independent gesture or animation state. */
internal object ShadowListUtilsPressFeedback {
  fun background(theme: ShadowListUtilsTheme, background: Drawable, mask: Drawable): Drawable {
    return if (theme.pressFeedback == ShadowListUtilsThemePressFeedback.RIPPLE) {
      RippleDrawable(ColorStateList.valueOf(theme.colors.fill), background, mask)
    } else background
  }
  fun alpha(pressed: Boolean, disabled: Boolean = false, ripple: Boolean = false, pressedOpacity: Float = 0.55f, disabledOpacity: Float = 0.4f): Float {
    return if (disabled) disabledOpacity else if (pressed && !ripple) pressedOpacity else 1f
  }
}
