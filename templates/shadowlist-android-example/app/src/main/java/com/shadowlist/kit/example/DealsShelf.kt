package com.shadowlist.kit.example

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.widget.HorizontalScrollView
import android.widget.LinearLayout
import android.widget.TextView

/* A native horizontal shelf that scrolls with the surrounding Explore list. */
class DealsShelf(context: Context) : HorizontalScrollView(context) {
  init {
    val theme = context.listTheme
    isHorizontalScrollBarEnabled = false
    val strip = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL; setPadding(0, 0, 0, theme.spacing.lg.dp) }
    val heading = LinearLayout(context).apply {
      orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER_VERTICAL; setPadding(theme.spacing.lg.dp, 0, theme.spacing.lg.dp, 0)
      addView(TextView(context).apply { text = "Deals"; theme.typography.title3.applyTo(this, theme.colors.label) })
      addView(TextView(context).apply { text = "Fares this week"; theme.typography.subhead.applyTo(this, theme.colors.secondaryLabel) })
    }
    strip.addView(heading, LinearLayout.LayoutParams(150.dp, 180.dp))
    val cities = listOf("Lisbon", "Reykjavik", "Kyoto", "Marrakesh", "Tromsø", "Oaxaca")
    val colors = listOf("#2F6FEB", "#8250DF", "#BF3989", "#D4A72C", "#1A7F37", "#CF222E")
    cities.forEachIndexed { index, city ->
      strip.addView(TextView(context).apply {
        text = "$city\nfrom $${89 + (index * 53) % 400}"
        theme.typography.subhead.applyTo(this, theme.colors.onAccent); gravity = Gravity.BOTTOM
        setPadding(theme.spacing.md.dp, theme.spacing.md.dp, theme.spacing.md.dp, theme.spacing.md.dp)
        background = GradientDrawable().apply { setColor(Color.parseColor(colors[index])); cornerRadius = 16.dpf }
      }, LinearLayout.LayoutParams((110 + (index * 37) % 150).dp, 164.dp).apply { setMargins(6.dp, 8.dp, 6.dp, 8.dp) })
    }
    addView(strip)
  }
}
