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
  private val theme = ShadowListUtilsTheme.system(context)
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
    append(ShadowListUtilsAssistantScrollButton(context, theme).apply {
      visible = true
      onPress = { scroll.smoothScrollTo(0, 0); status.text = "Returned to the first component" }
    })
  }
  private fun append(child: View) {
    content.addView(child, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { bottomMargin = 16.dp })
  }
}
