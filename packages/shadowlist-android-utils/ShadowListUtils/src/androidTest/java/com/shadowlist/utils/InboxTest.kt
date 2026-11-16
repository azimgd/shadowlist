package com.shadowlist.utils

import android.graphics.Rect
import android.graphics.drawable.ColorDrawable
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import androidx.core.graphics.ColorUtils
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.shadowlist.kit.ShadowListKitSwipeAction
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class InboxTest {
  private val theme = testTheme
  private val width = TEST_WIDTH_DP.px

  /*
   * Where a row's text starts: after the gutter, the avatar and the gap.
   */
  private val bodyLeft = (28f + 40f + 12f).px

  private fun message(index: Int, read: Boolean = true, flagged: Boolean = false) = ShadowListUtilsInboxMessage(
    key = "mail-$index", sender = "Sender $index", subject = "Subject $index",
    preview = "A preview that is long enough to wrap onto a second line of the row for certain.",
    receivedAtMs = System.currentTimeMillis().toDouble() - 5 * 60_000, read = read, flagged = flagged)

  private fun row(item: ShadowListUtilsInboxMessage, editing: Boolean = false) =
    ShadowListUtilsInboxRow(item, editing, theme, ShadowListUtilsInboxLabels())

  private fun inboxRow(list: ShadowListUtilsList, index: Int): ShadowListUtilsInboxRow? =
    list.rowView(index)?.row as? ShadowListUtilsInboxRow

  private fun layout(row: ShadowListUtilsInboxRow): ShadowListUtilsInboxRowLayout =
    row.layout(width) as ShadowListUtilsInboxRowLayout

  private fun backgroundColor(view: View): Int? = (view.background as? ColorDrawable)?.color

  /*
   * What a screen reader is told about a view.
   */
  private fun nodeInfo(view: View): AccessibilityNodeInfo =
    AccessibilityNodeInfo.obtain().also { view.onInitializeAccessibilityNodeInfo(it) }

  /*
   * The actions a row adds to its node. The platform's own actions have no label.
   */
  private fun customActions(view: View): List<AccessibilityNodeInfo.AccessibilityAction> =
    nodeInfo(view).actionList.filter { it.label != null }

  private fun items(menu: Menu): List<MenuItem> = List(menu.size()) { menu.getItem(it) }

  private fun titles(menu: Menu?): List<String> = items(requireNotNull(menu) { "no menu" }).map { it.title.toString() }

  /*
   * Chooses a menu entry the way a press on it does. The platform's menu offers that to its own
   * popup only, through the entry's invoke.
   */
  private fun choose(item: MenuItem): Boolean = item.javaClass.getMethod("invoke").invoke(item) as Boolean

  @Test
  fun the_list_shows_its_items() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    list.items = List(5) { message(it) }
    host(list)
    assertEquals(List(5) { "mail-$it" }, list.keys)
    assertTrue(list.rowView(0) is ShadowListUtilsInboxRowView)
  }

  @Test
  fun the_list_shows_separators_and_selects_only_when_asked() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    assertTrue(list.showsSeparators)
    assertEquals(68f.px, list.separatorInsetStartPx)
    assertEquals(theme.colors.separator, list.separatorColor)
    assertFalse(list.allowsSelection)
    assertFalse(list.allowsMultipleSelection)
    list.onPressItem = {}
    assertTrue(list.allowsSelection)
    list.onPressItem = null
    list.editing = true
    assertTrue(list.allowsSelection)
    assertTrue(list.allowsMultipleSelection)
    list.editing = false
    assertFalse(list.allowsSelection)
    assertFalse(list.allowsMultipleSelection)
  }

  @Test
  fun a_changed_item_is_configured_again_and_a_deleted_item_leaves() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    val items = MutableList(4) { message(it, read = false) }
    list.items = items.toList()
    host(list)
    val untouched = list.rowView(0)
    val untouchedRow = untouched?.row
    items[1] = items[1].copy(read = true)
    list.setItems(items.toList(), false)
    host(list)
    assertEquals(true, inboxRow(list, 1)?.item?.read)
    assertSame(untouched, list.rowView(0))
    assertSame(untouchedRow, list.rowView(0)?.row)
    items.removeAt(0)
    list.setItems(items.toList(), true)
    assertEquals(listOf("mail-1", "mail-2", "mail-3"), list.keys)
  }

  @Test
  fun a_tap_presses_an_item_while_the_list_is_not_editing() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    list.items = List(3) { message(it) }
    val pressed = ArrayList<String>()
    var changes = 0
    list.onPressItem = { pressed.add(it.key) }
    list.onSelectionChange = { ++changes }
    host(list)
    list.tap(2)
    assertEquals(listOf("mail-2"), pressed)
    assertEquals(0, changes)
    assertTrue(list.selectedKeys.isEmpty())
  }

  @Test
  fun a_tap_selects_while_editing_and_reports_the_selected_keys() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    list.items = List(4) { message(it) }
    host(list)
    var pressed = 0
    val changes = ArrayList<Set<String>>()
    list.onPressItem = { ++pressed }
    list.onSelectionChange = { changes.add(it) }
    list.setEditing(true, false)
    host(list)
    assertEquals(true, inboxRow(list, 0)?.editing)

    list.tap(1)
    list.tap(3)
    assertEquals(listOf(setOf("mail-1"), setOf("mail-1", "mail-3")), changes)
    assertEquals(setOf("mail-1", "mail-3"), list.selectedKeys)
    // A tap on a selected item of a list that is editing deselects it.
    list.tap(1)
    assertEquals(setOf("mail-3"), changes.last())
    assertEquals(0, pressed)
  }

  @Test
  fun setting_selected_keys_selects_without_a_callback() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    list.items = List(4) { message(it) }
    var changes = 0
    list.onSelectionChange = { ++changes }
    list.setEditing(true, false)
    list.selectedKeys = setOf("mail-0", "mail-2")
    assertEquals(listOf(0, 2), list.selectedIndices.sorted())
    list.selectedKeys = setOf("mail-3")
    assertEquals(listOf(3), list.selectedIndices.toList())
    assertEquals(0, changes)
    // The selection follows its keys through a change of the items.
    list.setItems(list.items.drop(1), false)
    assertEquals(setOf("mail-3"), list.selectedKeys)
    assertEquals(listOf(2), list.selectedIndices.toList())
    assertEquals(0, changes)
  }

  @Test
  fun leaving_editing_clears_the_selection() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    list.items = List(4) { message(it) }
    host(list)
    val changes = ArrayList<Set<String>>()
    list.onSelectionChange = { changes.add(it) }
    list.setEditing(true, false)
    list.selectedKeys = setOf("mail-1")
    list.setEditing(false, false)
    host(list)
    assertEquals(0, list.selectedIndices.size)
    assertEquals(emptySet<String>(), list.selectedKeys)
    assertEquals(listOf(emptySet<String>()), changes)
    assertEquals(false, list.rowView(1)?.isSelected)
    // Nothing was selected the second time, and nothing is reported.
    list.setEditing(true, false)
    list.setEditing(false, false)
    assertEquals(listOf(emptySet<String>()), changes)
  }

  @Test
  fun the_swipe_actions_follow_the_items_state_and_the_closures_that_are_set() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    list.items = listOf(message(0, read = true), message(1, read = false, flagged = true))
    assertTrue(list.leadingActions(0).isEmpty())
    assertTrue(list.trailingActions(0).isEmpty())
    val calls = ArrayList<String>()
    list.onToggleRead = { calls.add("read ${it.key}") }
    list.onToggleFlag = { calls.add("flag ${it.key}") }
    list.onDelete = { calls.add("delete ${it.key}") }
    assertEquals(listOf("Unread"), list.leadingActions(0).map { it.title })
    assertEquals(listOf("Read"), list.leadingActions(1).map { it.title })
    assertEquals(theme.colors.blue, list.leadingActions(0).first().backgroundColor)
    val trailing = list.trailingActions(1)
    assertEquals(listOf("Delete", "Unflag"), trailing.map { it.title })
    assertEquals(ShadowListKitSwipeAction.Style.DESTRUCTIVE, trailing.first().style)
    assertEquals(theme.colors.red, trailing.first().backgroundColor)
    assertEquals(theme.colors.orange, trailing.last().backgroundColor)
    assertEquals(listOf("Delete", "Flag"), list.trailingActions(0).map { it.title })
    // A full swipe runs the first action.
    assertEquals(true, list.adapter.trailingSwipeActionsForItem(list, 1)?.performsFirstActionWithFullSwipe)
    list.leadingActions(0).first().perform()
    trailing.first().perform()
    trailing.last().perform()
    assertEquals(listOf("read mail-0", "delete mail-1", "flag mail-1"), calls)
    list.onDelete = null
    assertEquals(listOf("Flag"), list.trailingActions(0).map { it.title })
    list.setEditing(true, false)
    assertTrue(list.leadingActions(0).isEmpty())
    assertTrue(list.trailingActions(0).isEmpty())
  }

  /*
   * The platform's menu has no title. The subject is its first entry, which cannot be chosen.
   */
  @Test
  fun the_menu_lists_the_actions_in_order_under_the_subject() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    list.items = listOf(message(0, read = true), message(1, read = false, flagged = true))
    assertNull(list.menu(0))
    list.onReply = {}
    list.onToggleRead = {}
    list.onToggleFlag = {}
    list.onDelete = {}
    list.onSelectionChange = {}
    val menu = requireNotNull(list.menu(0)) { "no menu" }
    assertEquals("Subject 0", menu.getItem(0).title.toString())
    assertFalse(menu.getItem(0).isEnabled)
    assertEquals(listOf("Reply", "Mark as Unread", "Flag", "Select", "Delete"), titles(menu).drop(1))
    assertTrue(items(menu).drop(1).all { it.isEnabled })
    assertEquals(listOf("Subject 1", "Reply", "Mark as Read", "Unflag", "Select", "Delete"), titles(list.menu(1)))
    list.onReply = null
    list.onSelectionChange = null
    assertEquals(listOf("Subject 0", "Mark as Unread", "Flag", "Delete"), titles(list.menu(0)))
    list.setEditing(true, false)
    assertNull(list.menu(0))
  }

  @Test
  fun the_menus_select_entry_reports_its_one_key() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    list.items = List(3) { message(it) }
    host(list)
    val selected = ArrayList<Set<String>>()
    list.onSelectionChange = { selected.add(it) }
    val view = requireNotNull(list.rowView(2)) { "item 2 is not mounted" }
    val actions = customActions(view)
    assertEquals(listOf("Select"), actions.map { it.label.toString() })
    assertTrue(view.performAccessibilityAction(actions[0].id, null))
    assertEquals(listOf(setOf("mail-2")), selected)
    assertFalse(list.editing)
  }

  @Test
  fun an_action_acts_on_the_item_as_it_is_when_it_runs() = onMain {
    val list = ShadowListUtilsInboxList(testContext, theme)
    host(list)
    val items = mutableListOf(message(0, read = false), message(1))
    list.items = items.toList()
    host(list)
    val toggled = ArrayList<ShadowListUtilsInboxMessage>()
    val deleted = ArrayList<String>()
    list.onToggleRead = { toggled.add(it) }
    list.onDelete = { deleted.add(it.key) }
    val swipe = list.leadingActions(0).first()
    val menu = requireNotNull(list.menu(0)) { "no menu" }
    val delete = list.trailingActions(1).first()
    // The message changes and another one leaves while the actions are open.
    items[0] = items[0].copy(subject = "Edited")
    items.removeAt(1)
    list.items = items.toList()
    swipe.perform()
    assertEquals(listOf("Edited"), toggled.map { it.subject })
    // The entry after the subject.
    assertTrue(choose(menu.getItem(1)))
    assertEquals(listOf("Edited", "Edited"), toggled.map { it.subject })
    delete.perform()
    assertEquals(emptyList<String>(), deleted)
  }

  @Test
  fun the_row_layout_holds_its_metrics() {
    val typography = theme.typography
    val bodyLine = typography.body.scaledLineHeightPx()
    val subheadLine = typography.subhead.scaledLineHeightPx()
    val layout = layout(row(message(0, read = false, flagged = true)))
    val dotLeft = (28f.px - 10f.px) / 2
    assertEquals(Rect(dotLeft, 12f.px + 16f.px, dotLeft + 10f.px, 12f.px + 16f.px + 10f.px), layout.unreadDotFrame)
    val checkLeft = (28f.px - 22f.px) / 2
    assertEquals(Rect(checkLeft, 12f.px + 9f.px, checkLeft + 22f.px, 12f.px + 9f.px + 22f.px), layout.checkFrame)
    assertEquals(Rect(28f.px, 12f.px, 28f.px + 40f.px, 12f.px + 40f.px), layout.avatarFrame)
    assertEquals(bodyLeft, layout.senderFrame.left)
    assertEquals(12f.px, layout.senderFrame.top)
    assertEquals(width - 16f.px, layout.timeFrame.right)
    assertEquals(8f.px, layout.flagFrame.width())
    assertEquals(8f.px, layout.flagFrame.height())
    assertEquals(layout.timeFrame.left - 4f.px, layout.flagFrame.right)
    assertEquals(12f.px + (bodyLine - 8f.px) / 2, layout.flagFrame.top)
    assertEquals(bodyLeft, layout.subjectFrame.left)
    assertEquals(12f.px + bodyLine, layout.subjectFrame.top)
    assertEquals(bodyLeft, layout.previewFrame.left)
    assertEquals(12f.px + bodyLine + subheadLine, layout.previewFrame.top)
    assertEquals(2, layout.preview.lineCount)
    assertEquals(12f.px + bodyLine + subheadLine * 3 + 12f.px, layout.heightPx)
    assertEquals(1, layout.sender.lineCount)
    assertEquals(1, layout.subject.lineCount)
    assertLines(layout.sender, typography.body, 1)
    assertLines(layout.subject, typography.subhead, 1)
    assertLines(layout.preview, typography.subhead, 2)
    assertEquals("5m", layout.time.layout.text.toString())
  }

  @Test
  fun a_right_to_left_row_mirrors_its_frames() {
    val mail = row(message(0, read = false, flagged = true))
    val plain = layout(mail)
    val mirrored = withRightToLeft { layout(mail) }
    assertEquals(width - plain.avatarFrame.right, mirrored.avatarFrame.left)
    assertEquals(width - plain.unreadDotFrame.right, mirrored.unreadDotFrame.left)
    assertEquals(width - bodyLeft, mirrored.senderFrame.right)
    assertEquals(16f.px, mirrored.timeFrame.left)
    assertEquals(mirrored.timeFrame.right + 4f.px, mirrored.flagFrame.left)
  }

  @Test
  fun the_row_reads_its_states_and_shows_its_selection_while_editing() = onMain {
    // The fill color has an alpha and lies over the resting background.
    val pressedColor = ColorUtils.compositeColors(theme.colors.fill, theme.colors.background)
    val view = ShadowListUtilsInboxRowView(testContext)
    val item = message(0, read = false, flagged = true)
    view.configure(row(item), null)
    assertEquals("Unread, Flagged, Sender 0, Subject 0, ${item.preview}", view.contentDescription)
    assertEquals(theme.colors.background, backgroundColor(view))
    view.setSelected(true, false)
    assertEquals(theme.colors.background, backgroundColor(view))

    view.configure(row(message(1), editing = true), null)
    assertEquals(pressedColor, backgroundColor(view))
    assertEquals("Selected, Sender 1, Subject 1, ${item.preview}", view.contentDescription)
    assertTrue(nodeInfo(view).isSelected)
    view.setSelected(false, false)
    assertEquals(theme.colors.background, backgroundColor(view))
    assertFalse(nodeInfo(view).isSelected)
    view.setHighlighted(true, false)
    assertEquals(pressedColor, backgroundColor(view))
  }

  @Test
  fun the_age_comes_from_the_labels() {
    assertEquals("5m", row(message(0)).time)
    val labels = ShadowListUtilsInboxLabels().let { it.copy(time = it.time.copy(minutes = { count -> "$count min ago" })) }
    assertEquals("5 min ago", ShadowListUtilsInboxRow(message(0), false, theme, labels).time)
  }
}
