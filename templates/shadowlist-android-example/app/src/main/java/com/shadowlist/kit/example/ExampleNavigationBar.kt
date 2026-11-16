package com.shadowlist.kit.example

import android.content.Context
import android.graphics.Typeface
import android.view.Gravity
import android.widget.FrameLayout
import android.widget.ImageButton
import android.widget.TextView
import android.util.TypedValue
import kotlin.math.roundToInt

/* Matches the centered native-stack header used by the Fabric Android example. */
class ExampleNavigationBar(
  context: Context,
  title: String,
  home: Boolean,
  onBack: () -> Unit,
  onMore: ((android.view.View) -> Unit)?,
) : FrameLayout(context) {
  private val heading = TextView(context)

  init {
    val theme = context.listTheme
    setBackgroundColor(if (home) theme.colors.groupedBackground else theme.colors.background)
    if (!home) elevation = 3.dp.toFloat()
    heading.apply {
      text = title
      setTextColor(theme.colors.label)
      setTextSize(TypedValue.COMPLEX_UNIT_PX, kotlin.math.ceil(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 20f, resources.displayMetrics)))
      typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
      letterSpacing = 0f
      paint.isSubpixelText = true
      gravity = Gravity.CENTER_VERTICAL
      maxLines = 1
      ellipsize = android.text.TextUtils.TruncateAt.END
      importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES
      androidx.core.view.ViewCompat.setAccessibilityHeading(this, true)
    }
    addView(heading, LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT, Gravity.CENTER_HORIZONTAL).apply {
      marginStart = 56.dp; marginEnd = 56.dp
    })
    if (!home) {
      addView(ImageButton(context).apply {
        setImageResource(R.drawable.ic_example_back)
        imageTintList = android.content.res.ColorStateList.valueOf(theme.colors.accent)
        setBackgroundResource(android.R.drawable.list_selector_background)
        contentDescription = "Navigate up"
        setOnClickListener { onBack() }
      }, LayoutParams(56.dp, LayoutParams.MATCH_PARENT, Gravity.START))
    }
    if (onMore != null) {
      addView(ImageButton(context).apply {
        setImageResource(R.drawable.ic_example_more)
        imageTintList = android.content.res.ColorStateList.valueOf(theme.colors.accent)
        setBackgroundResource(android.R.drawable.list_selector_background)
        contentDescription = "More"
        setOnClickListener(onMore)
      }, LayoutParams(40.dp, 40.dp, Gravity.END or Gravity.CENTER_VERTICAL).apply { marginEnd = 16.dp })
    }
  }

  /*
   * The title is as wide as its text. Its origin is rounded to a pixel, the way React Native
   * places the header's title, where a centering gravity would floor it.
   */
  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    super.onLayout(changed, left, top, right, bottom)
    val headingLeft = ((width - heading.measuredWidth) / 2f).roundToInt()
    heading.layout(headingLeft, heading.top, headingLeft + heading.measuredWidth, heading.bottom)
  }
}
