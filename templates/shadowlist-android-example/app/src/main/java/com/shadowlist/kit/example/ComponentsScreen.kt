package com.shadowlist.kit.example

import android.content.Context
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import com.shadowlist.utils.*

/* Real public-component consumer. Callbacks change application state, never library models. */
class ComponentsScreen(private val context: Context) : Screen {
  private val theme = ShadowListUtilsTheme.LIGHT
  private val scroll = ScrollView(context)
  private val content = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL; setPadding(16.dp, 0, 16.dp, 24.dp) }
  private val status = TextView(context).apply { text = "Choose a suggestion"; setTextColor(theme.colors.label) }
  private val thinking = ShadowListUtilsAssistantThinking(context, "Compare the travel dates, available seats and the total fare before suggesting a route.", true, theme)
  private val tool = ShadowListUtilsAssistantToolCallCard(context, ShadowListUtilsAssistantToolCall("search", "Search fares", "Lisbon, three days", status = ShadowListUtilsAssistantToolStatus.Running), theme)
  private val typing = ShadowListUtilsAssistantTypingIndicator(context, theme)
  private val footer = ShadowListUtilsPollFooter(context, 3.0, theme)
  override val view: View get() = scroll
  override fun load() {
    scroll.setBackgroundColor(theme.colors.background); scroll.addView(content)
    append(status)
    val empty = ShadowListUtilsAssistantEmpty(context, theme)
    empty.suggestions = listOf(ShadowListUtilsAssistantSuggestion("Plan a trip", "Three days in Lisbon"), ShadowListUtilsAssistantSuggestion("Compare fares", "Find a quieter travel day"))
    empty.onSelectSuggestion = { status.text = "Selected: ${it.prompt}" }
    append(empty); append(thinking); append(tool); append(typing); append(footer)
    append(Button(context).apply {
      text = "Finish search and add a vote"
      setOnClickListener {
        thinking.active = false; thinking.thinkingMs = 1800.0
        tool.call = ShadowListUtilsAssistantToolCall("search", "Search fares", "Lisbon, three days", status = ShadowListUtilsAssistantToolStatus.Done("Three routes are available. The quieter option leaves on Tuesday."))
        typing.visibility = View.GONE; footer.totalVotes += 1
        status.text = "Search complete; vote total ${footer.totalVotes.toInt()}"
      }
    })
    val accountValue = ShadowListUtilsFormValue(context, "3 saved routes", theme)
    val save = ShadowListUtilsFormButton(context, "Save route", theme).apply {
      onPress = { accountValue.text = "4 saved routes"; status.text = "Route saved through Form.Button" }
    }
    val delete = ShadowListUtilsFormButton(context, "Remove saved route", theme).apply {
      destructive = true
      onPress = { accountValue.text = "3 saved routes"; status.text = "Saved route removed" }
    }
    append(ShadowListUtilsGroupedCaption(context, "Saved routes", theme))
    append(ShadowListUtilsFormCard(context, listOf(save, delete), theme))
    append(ShadowListUtilsGroupedCard(context, listOf(accountValue, ShadowListUtilsGroupedSeparator(context, theme = theme)), theme))
    append(ShadowListUtilsGroupedSection(context, "Travel account", listOf(ShadowListUtilsFormValue(context, "Lisbon · Tuesday", theme)), theme))
    val routeName = ShadowListUtilsFormInput(context, "Lisbon", "Route name", theme).apply {
      placeholder = "Route name"
      returnKeyType = ShadowListUtilsReturnKeyType.DONE
      onChange = { value = it }
      onSubmitEditing = { status.text = "Route submitted: $value" }
    }
    val budget = ShadowListUtilsFormInput(context, "24", "Budget", theme).apply {
      numeric = true
      onChange = { value = it }
    }
    append(ShadowListUtilsFormCard(context, listOf(
      ShadowListUtilsFormRow(context, "Route", listOf(routeName), theme),
      ShadowListUtilsFormRow(context, "Budget", listOf(budget), theme),
    ), theme))
    append(ShadowListUtilsSearchField(context, "", theme).apply {
      labels = ShadowListUtilsSearchFieldLabels("Filter routes")
      onChange = { value = it; status.text = "Filter: $it" }
    })
    append(ShadowListUtilsTextField(context, "", theme).apply {
      placeholder = "Travel notes"
      accessibilityLabel = "Travel notes"
      multiline = true
      onChange = { value = it }
    })
    val rippleTheme = theme.copy(pressFeedback = ShadowListUtilsThemePressFeedback.RIPPLE)
    val badge = ShadowListUtilsIndexBadge(context, 1, theme)
    val proceed = ShadowListUtilsPillButton(context, "Continue", rippleTheme)
    proceed.onPress = { proceed.disabled = true; status.text = "Route ready; add another route to continue again" }
    val add = ShadowListUtilsDashedButton(context, "Add route", theme).apply {
      onPress = { badge.index += 1; proceed.disabled = false; status.text = "Added route ${badge.index}" }
    }
    val glyph = TextView(context).apply { text = "⋯"; importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO; textSize = 22f; includeFontPadding = false; setTextColor(theme.colors.accent) }
    val more = ShadowListUtilsIconButton(context, glyph, "More routes", rippleTheme).apply {
      onPress = { status.text = "More routes requested" }
    }
    val buttons = LinearLayout(context).apply {
      orientation = LinearLayout.HORIZONTAL; gravity = android.view.Gravity.CENTER_VERTICAL
      addView(badge, LinearLayout.LayoutParams(22.dp, 22.dp))
      addView(proceed, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, 40.dp).apply { marginStart = 12.dp })
      addView(more, LinearLayout.LayoutParams(40.dp, 40.dp).apply { marginStart = 12.dp })
    }
    append(buttons); append(add)
    val direct = ShadowListUtilsChip(context, "Direct routes", theme).apply { isSelected = true; maxFontSizeMultiplier = 1.5f }
    val seat = ShadowListUtilsChip(context, "Seat 12A", theme).apply {
      variant = ShadowListUtilsChipVariant.PLAIN; size = ShadowListUtilsChipSize.COMPACT; mono = true
    }
    val train = ShadowListUtilsChoiceChip(context, "Train", theme.colors.fill, true, theme)
    val flight = ShadowListUtilsChoiceChip(context, "Flight", theme.colors.accentSoft, false, theme)
    val chips = ShadowListUtilsChipRow(context, listOf(direct, seat, train, flight), theme)
    direct.onPress = { direct.isSelected = !direct.isSelected; status.text = if (direct.isSelected) "Direct routes selected" else "Direct routes cleared" }
    direct.onClear = { chips.children = chips.children.filter { it !== direct }; status.text = "Direct routes removed" }
    seat.onClear = { chips.children = chips.children.filter { it !== seat }; status.text = "Seat preference removed" }
    train.onPress = { train.isSelected = true; flight.isSelected = false; status.text = "Train selected" }
    flight.onPress = { train.isSelected = false; flight.isSelected = true; status.text = "Flight selected" }
    append(chips)
    val latest = ShadowListUtilsAssistantScrollButton(context, theme).apply {
      visible = true
      onPress = { scroll.smoothScrollTo(0, 0); status.text = "Returned to the first component" }
    }
    content.addView(latest, LinearLayout.LayoutParams(36.dp, 36.dp).apply { gravity = android.view.Gravity.CENTER_HORIZONTAL; bottomMargin = 16.dp })
  }
  private fun append(child: View) {
    content.addView(child, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { bottomMargin = 16.dp })
  }
}
