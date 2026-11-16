package com.shadowlist.kit.example

import android.content.Context
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import com.shadowlist.utils.ShadowListUtilsAvatar
import com.shadowlist.utils.ShadowListUtilsContactItem
import com.shadowlist.utils.ShadowListUtilsGroupedCard

class ContactDetailScreen(context: Context, contact: ShadowListUtilsContactItem) : Screen {
  private val theme = context.listTheme
  private val content = LinearLayout(context).apply {
    orientation = LinearLayout.VERTICAL
    setPadding(0, theme.spacing.lg.dp, 0, theme.spacing.lg.dp)
  }
  override val view: View = ScrollView(context).apply {
    setBackgroundColor(theme.colors.groupedBackground)
    addView(content, android.widget.FrameLayout.LayoutParams(android.widget.FrameLayout.LayoutParams.MATCH_PARENT, android.widget.FrameLayout.LayoutParams.WRAP_CONTENT))
  }

  init {
    val hero = LinearLayout(context).apply {
      orientation = LinearLayout.VERTICAL
      gravity = Gravity.CENTER_HORIZONTAL
      setPadding(theme.spacing.lg.dp, theme.spacing.lg.dp, theme.spacing.lg.dp, theme.spacing.lg.dp)
    }
    val avatar = ShadowListUtilsAvatar(context, 96f, theme)
    avatar.show(contact.name, contact.avatarUrl, contact.avatarColor)
    hero.addView(avatar, LinearLayout.LayoutParams(96.dp, 96.dp))
    val name = TextView(context).apply {
      text = contact.name
      theme.typography.title2.applyTo(this, theme.colors.label)
      gravity = Gravity.CENTER
      androidx.core.view.ViewCompat.setAccessibilityHeading(this, true)
    }
    hero.addView(name, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
      topMargin = theme.spacing.md.dp
    })
    content.addView(hero, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
    contact.subtitle?.takeIf { it.isNotEmpty() }?.let { phone ->
      val row = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setPadding(theme.grouped.rowInset.dp, 11.dp, theme.grouped.rowInset.dp, 11.dp)
      }
      row.addView(TextView(context).apply {
        text = "mobile"
        theme.typography.subhead.applyTo(this, theme.colors.label)
      })
      row.addView(TextView(context).apply {
        text = phone
        theme.typography.body.applyTo(this, theme.colors.accent)
        setTextIsSelectable(true)
      }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
        topMargin = theme.spacing.xxs.dp
      })
      content.addView(ShadowListUtilsGroupedCard(context, listOf(row), theme),
        LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
          topMargin = theme.spacing.lg.dp
        })
    }
  }

  override fun load() {}
}
