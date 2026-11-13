package com.shadowlist.utils

import android.view.accessibility.AccessibilityNodeInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ActivityTest {
  private val theme = testTheme
  private fun item(key: String = "activity", read: Boolean = false) = ShadowListUtilsActivityItem(
    key, ShadowListUtilsActivityActor("Ada Lovelace"), "commented on your itinerary",
    "A long activity preview that wraps over multiple lines on a phone.", 0.0, read)

  @Test fun the_action_uses_regular_metrics_while_the_actor_keeps_its_emphasis() = onMain {
    val item = ShadowListUtilsActivityItem("one", ShadowListUtilsActivityActor("Mara"),
      "followed a remarkably wonderful itinerary", createdAtMs = 0.0)
    val row = ShadowListUtilsActivityRow(item, theme = theme, formatTime = { "" })
    val layout = row.layout(2000f.px) as ShadowListUtilsActivityRowLayout
    val text = "${item.actor.name} ${item.action}"
    val regular = theme.typography.subhead.layout(text, 2000f.px)
    val semibold = theme.typography.subhead.with(ShadowListUtilsFontWeight.SEMIBOLD).layout(text, 2000f.px)
    assertTrue(layout.title.width > regular.width)
    assertTrue(layout.title.width < semibold.width)
    val narrow = withTextSize(1.6f) { row.layout(230f.px) as ShadowListUtilsActivityRowLayout }
    assertTrue(narrow.title.lineCount > 1)
  }

  @Test
  fun the_list_shows_its_items_and_selects_only_when_asked() = onMain {
    val list = ShadowListUtilsActivityList(testContext, theme)
    list.items = listOf(item("one"), item("two"))
    host(list)
    assertEquals(listOf("one", "two"), list.keys)
    assertTrue(list.rowView(0) is ShadowListUtilsActivityRowView)
    assertFalse(list.allowsSelection)
    val pressed = ArrayList<String>()
    list.onPressItem = { pressed.add(it.key) }
    host(list)
    list.tap(0)
    assertEquals(listOf("one"), pressed)
    assertFalse(list.items[0].read)
    list.onPressItem = null
    assertFalse(list.allowsSelection)
  }

  @Test
  fun read_and_label_changes_replace_the_visible_state() = onMain {
    val list = ShadowListUtilsActivityList(testContext, theme)
    list.formatTime = { "5m" }
    list.items = listOf(item())
    host(list)
    assertTrue(requireNotNull(list.rowView(0)).contentDescription.toString().contains("Unread"))
    assertTrue(requireNotNull(list.rowView(0)).contentDescription.toString().contains("5m"))
    list.items = listOf(item(read = true))
    host(list)
    assertFalse(requireNotNull(list.rowView(0)).contentDescription.toString().contains("Unread"))
    list.items = listOf(item())
    list.labels = list.labels.copy(unread = "New activity")
    host(list)
    assertTrue(requireNotNull(list.rowView(0)).contentDescription.toString().contains("New activity"))
    val info = AccessibilityNodeInfo.obtain()
    requireNotNull(list.rowView(0)).onInitializeAccessibilityNodeInfo(info)
    assertFalse(info.isClickable)
  }

  @Test
  fun an_absent_preview_has_no_text_and_large_text_grows_the_row() = onMain {
    val row = ShadowListUtilsActivityRow(item(), theme = theme, formatTime = { "now" })
    val regular = row.layout(TEST_WIDTH_DP.px) as ShadowListUtilsActivityRowLayout
    val large = withTextSize(1.6f) { row.layout(TEST_WIDTH_DP.px) as ShadowListUtilsActivityRowLayout }
    assertTrue(large.heightPx > regular.heightPx)
    assertTrue(large.title.lineCount > regular.title.lineCount)
    val empty = ShadowListUtilsActivityRow(item().copy(text = null), theme = theme).layout(TEST_WIDTH_DP.px) as ShadowListUtilsActivityRowLayout
    assertNull(empty.text)
    assertTrue(empty.textFrame.isEmpty)
  }

  @Test
  fun a_right_to_left_row_mirrors_its_frames() = onMain {
    val row = ShadowListUtilsActivityRow(item(), theme = theme, formatTime = { "now" })
    val regular = row.layout(TEST_WIDTH_DP.px) as ShadowListUtilsActivityRowLayout
    val mirrored = withRightToLeft { row.layout(TEST_WIDTH_DP.px) as ShadowListUtilsActivityRowLayout }
    assertEquals(regular.avatarFrame.left, TEST_WIDTH_DP.px - mirrored.avatarFrame.right)
    assertEquals(regular.titleFrame.left, TEST_WIDTH_DP.px - mirrored.titleFrame.right)
    assertEquals(regular.timeFrame.left, TEST_WIDTH_DP.px - mirrored.timeFrame.right)
  }
}
