package com.shadowlist.kit.example

import android.content.Context
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import com.shadowlist.utils.*
import kotlin.math.floor

/* Application-owned cards preserve the board's native drag and accessibility behavior. */
class BoardScreen(private val context: Context) : Screen {
  private data class Card(val key: String, val title: String, val label: String, val due: String)
  private var serial = 20
  private var columns = listOf(
    ShadowListUtilsBoardColumn("backlog", "Backlog", listOf(
      Card("c1", "Seat map shows the wrong row letters", "Bug", "Oct 28"),
      Card("c2", "Boarding pass dark mode", "Design", "Oct 30"),
      Card("c3", "Interview five frequent flyers about rebooking", "Research", "Nov 2"),
      Card("c4", "Rotate the push certificate", "Ops", "Nov 4"),
      Card("c5", "Rewrite the delay notice so it says what to do next", "Copy", "Nov 5"),
      Card("c6", "Gate change banner", "Design", "Nov 7"),
      Card("c7", "Lounge pass expires a day early in Tokyo", "Bug", "Nov 8"),
      Card("c8", "Baggage tracking survey", "Research", "Nov 11"))),
    ShadowListUtilsBoardColumn("doing", "In progress", listOf(
      Card("c9", "Trip timeline widget", "Design", "Oct 27"),
      Card("c10", "Check-in crashes when the passport scan is cancelled", "Bug", "Oct 27"),
      Card("c11", "Move receipts to the new store", "Ops", "Oct 29"),
      Card("c12", "Onboarding headlines", "Copy", "Oct 31"))),
    ShadowListUtilsBoardColumn("done", "Done", listOf(
      Card("c13", "Upgrade offer sheet", "Design", "Oct 20"),
      Card("c14", "Wallet pass loses the gate after an update", "Bug", "Oct 21"),
      Card("c15", "Status page on the new host", "Ops", "Oct 23"))))
  private val theme = context.listTheme
  private val root = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
  private val board = ShadowListUtilsBoardList(context, keyExtractor = { card: Card -> card.key }, rowForItem = {
    ShadowListUtilsBoardCard(it.key, it.title, label = it.label, detail = it.due, color = color(it.label), theme = theme)
  }, theme = theme)
  override val view: View get() = root

  override fun load() {
    root.addView(ShadowListUtilsListFooter(context, "Touch and hold a card, then drag it up, down or into another column.", theme))
    // React Native rounds the board's bottom edge on the screen, which leaves the floor of the padding under it.
    root.addView(board, LinearLayout.LayoutParams(-1, 0, 1f).apply { bottomMargin = floor(theme.spacing.xl.dpf).toInt() })
    board.renderColumnHeader = { column ->
      LinearLayout(context).apply {
        // React Native floors a text origin to the pixel grid and rounds the vertical padding once.
        val top = floor(theme.spacing.md.dpf).toInt()
        gravity = Gravity.CENTER_VERTICAL; setPadding(theme.spacing.lg.dp, top, theme.spacing.lg.dp, (theme.spacing.md + theme.spacing.sm).dp - top)
        addView(TextView(context).apply { text = column.title; theme.typography.headline.applyTo(this, theme.colors.label) }, LinearLayout.LayoutParams(0, -2, 1f))
        addView(TextView(context).apply { text = column.items.size.toString(); theme.typography.subhead.with(ShadowListUtilsFontWeight.SEMIBOLD).applyTo(this, theme.colors.secondaryLabel) })
      }
    }
    board.renderColumnFooter = { column ->
      LinearLayout(context).apply {
        // The button's edges round as offsets from the footer's top, the way React Native places them.
        val top = theme.spacing.md.dp
        val bottom = (theme.spacing.md + theme.tapTarget).dp
        setPadding(theme.spacing.md.dp, top, theme.spacing.md.dp, (2 * theme.spacing.md + theme.tapTarget).dp - bottom)
        addView(ShadowListUtilsDashedButton(context, "Add card", theme).apply {
          onPress = {
            columns = columns.map { if (it.key == column.key) it.copy(items = it.items + Card("c${serial++}", "New travel idea", "Design", "Today")) else it }
            board.setColumns(columns, true)
          }
        }, LinearLayout.LayoutParams(-1, bottom - top))
      }
    }
    board.onMoveItem = { move ->
      columns = shadowListUtilsMoveBoardItem(columns, move) { it.key }
      board.setColumns(columns, true)
    }
    board.columns = columns
  }

  private fun color(label: String): Int = when (label) {
    "Bug" -> theme.colors.red
    "Design" -> theme.colors.accent
    "Research" -> theme.colors.green
    "Ops" -> theme.colors.orange
    else -> theme.colors.secondaryLabel
  }
}
