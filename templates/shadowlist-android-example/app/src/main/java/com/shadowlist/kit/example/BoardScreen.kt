package com.shadowlist.kit.example

import android.content.Context
import android.view.View
import com.shadowlist.utils.ShadowListUtilsTheme
import com.shadowlist.utils.ShadowListUtilsBoardCard
import com.shadowlist.utils.ShadowListUtilsBoardColumn
import com.shadowlist.utils.ShadowListUtilsBoardList
import com.shadowlist.utils.shadowListUtilsMoveBoardItem

/*
 * Generic application cards on a controlled board. The app applies each shared move plan.
 */
class BoardScreen(context: Context) : Screen {
  private data class Card(val key: String, val title: String, val label: String)
  private var columns = listOf(
    ShadowListUtilsBoardColumn("todo", "To do", listOf(Card("a", "Plan the journey", "Travel"), Card("b", "Book the window seat", "Flight"))),
    ShadowListUtilsBoardColumn("progress", "In progress", listOf(Card("c", "Pack the camera", "Equipment"))),
    ShadowListUtilsBoardColumn<Card>("done", "Done", emptyList()))
  private val theme = ShadowListUtilsTheme.system(context)
  private val board = ShadowListUtilsBoardList(context, keyExtractor = { card: Card -> card.key }, rowForItem = {
    ShadowListUtilsBoardCard(it.key, it.title, label = it.label, color = theme.colors.accent, theme = theme)
  }, theme = theme)
  override val view: View get() = board

  override fun load() {
    board.onMoveItem = { move ->
      columns = shadowListUtilsMoveBoardItem(columns, move) { it.key }
      board.setColumns(columns, true)
    }
    board.columns = columns
  }
}
