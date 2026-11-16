package com.shadowlist.kit.example

import android.content.Context
import android.widget.LinearLayout
import android.widget.ScrollView
import com.shadowlist.utils.ShadowListUtilsGroupedRow
import com.shadowlist.utils.ShadowListUtilsGroupedSection

class HomeScreen(context: Context, engineTitle: String, onOpen: (String) -> Unit, onSelectEngine: (android.view.View) -> Unit) : ScrollView(context) {
  init {
    val theme = context.listTheme
    setBackgroundColor(theme.colors.groupedBackground)
    isFillViewport = true
    val sections = LinearLayout(context).apply {
      orientation = LinearLayout.VERTICAL
      setPadding(0, theme.spacing.xxl.dp, 0, (theme.spacing.xxl * 2).dp)
    }
    for (section in ExampleSection.all) {
      val rows = section.examples.mapIndexed { index, example ->
        ShadowListUtilsGroupedRow(context, example.title, theme).apply {
          subtitle = example.summary
          subtitleLines = 2
          separated = index > 0
          chevron = true
          testID = example.route
          onPress = { onOpen(example.route) }
        }
      }
      sections.addView(ShadowListUtilsGroupedSection(context, section.title, rows, theme))
    }
    val engine = ShadowListUtilsGroupedRow(context, "List engine", theme).apply {
      subtitle = engineTitle
      chevron = true
    }
    engine.onPress = { onSelectEngine(engine) }
    sections.addView(ShadowListUtilsGroupedSection(context, children = listOf(engine), theme = theme))
    addView(sections, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
  }
}
