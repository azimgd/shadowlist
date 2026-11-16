package com.shadowlist.utils

import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class PollTest {
  private val theme = testTheme
  private val options = listOf(ShadowListUtilsPollOption("one", "Window seat", 1.0), ShadowListUtilsPollOption("two", "Aisle seat", 7.0))

  @Test fun the_default_chip_uses_the_first_unicode_scalar_and_an_icon_changes_the_row_version() = onMain {
    for ((text, expected) in listOf("Lisbon" to "L", "éclair" to "É", "e\u0301clair" to "E", "👩‍✈️ crew" to "👩")) {
      val row = ShadowListUtilsPollRow(ShadowListUtilsPollOption("one", text, 1.0), 1.0, theme = theme)
      val layout = row.layout(TEST_WIDTH_DP.px) as ShadowListUtilsPollRowLayout
      assertEquals(expected, layout.initial.layout.text.toString())
      assertEquals(44f.px, layout.selectionFrame.width())
    }
    val item = ShadowListUtilsPollOption("one", "Lisbon", 1.0)
    val first = ShadowListUtilsPollRow(item, 1.0, theme = theme)
    val second = ShadowListUtilsPollRow(item.copy(icon = android.graphics.drawable.ColorDrawable(0xFF000000.toInt())), 1.0, theme = theme)
    assertTrue(first.contentVersion != second.contentVersion)
  }

  @Test
  fun the_core_answers_every_shared_poll_vector() {
    for (entry in Vectors.cases("poll.json")) {
      val votes = entry.getJSONObject("input").getJSONArray("votes")
      val expected = entry.getJSONObject("output")
      val tally = DoubleArray(ShadowListUtilsCore.POLL_OUT_SLOTS)
      ShadowListUtilsCore.pollTally(DoubleArray(votes.length()) { votes.getDouble(it) }, tally)
      assertEquals(expected.getDouble("totalVotes"), tally[ShadowListUtilsCore.POLL_OUT_TOTAL_VOTES], 0.0)
      assertEquals(expected.getInt("leadingIndex"), tally[ShadowListUtilsCore.POLL_OUT_LEADING_INDEX].toInt())
      val percents = expected.getJSONArray("percents")
      for (index in 0 until votes.length()) {
        assertEquals(percents.getInt(index), ShadowListUtilsCore.pollPercent(votes.getDouble(index), tally[ShadowListUtilsCore.POLL_OUT_TOTAL_VOTES]))
      }
    }
  }

  @Test
  fun invalid_votes_use_the_same_core_result_without_platform_sanitizing() {
    val tally = DoubleArray(ShadowListUtilsCore.POLL_OUT_SLOTS)
    ShadowListUtilsCore.pollTally(doubleArrayOf(), tally)
    assertEquals(-1, tally[ShadowListUtilsCore.POLL_OUT_LEADING_INDEX].toInt())
    ShadowListUtilsCore.pollTally(doubleArrayOf(Double.NaN, 1.0), tally)
    assertTrue(tally[ShadowListUtilsCore.POLL_OUT_TOTAL_VOTES].isNaN())
    assertEquals(0, tally[ShadowListUtilsCore.POLL_OUT_LEADING_INDEX].toInt())
    assertEquals(0, ShadowListUtilsCore.pollPercent(1.0, tally[ShadowListUtilsCore.POLL_OUT_TOTAL_VOTES]))
    ShadowListUtilsCore.pollTally(doubleArrayOf(-2.0, 1.0), tally)
    assertEquals(-1.0, tally[ShadowListUtilsCore.POLL_OUT_TOTAL_VOTES], 0.0)
    assertEquals(1, tally[ShadowListUtilsCore.POLL_OUT_LEADING_INDEX].toInt())
  }

  @Test
  fun pressing_an_option_is_controlled_by_the_application() = onMain {
    val list = ShadowListUtilsPollList(testContext, theme)
    list.poll = ShadowListUtilsPollData("Where do you sit?", options)
    host(list)
    assertFalse(list.allowsSelection)
    assertEquals(listOf("one", "two"), list.keys)
    val pressed = ArrayList<String>()
    list.onPressItem = { pressed.add(it.key) }
    host(list)
    list.tap(0)
    assertEquals(listOf("one"), pressed)
    assertNull(list.poll.selectedKey)
    assertEquals(options, list.poll.options)
    list.setPoll(list.poll.copy(selectedKey = "one", options = listOf(options[0].copy(votes = 7.0), options[1])), false)
    host(list)
    val first = requireNotNull(list.rowView(0)).row as ShadowListUtilsPollRow
    val second = requireNotNull(list.rowView(1)).row as ShadowListUtilsPollRow
    assertTrue(first.isSelected)
    assertTrue(first.isLeading)
    assertEquals(50, first.percent)
    assertEquals(50, second.percent)
    list.onPressItem = null
    assertFalse(list.allowsSelection)
  }

  @Test
  fun labels_and_default_question_and_totals_follow_the_model_but_custom_views_stay() = onMain {
    val list = ShadowListUtilsPollList(testContext, theme)
    list.labels = ShadowListUtilsPollLabels(totalVotes = { "Total $it" }, option = { label, percent -> "$label has $percent percent" })
    list.poll = ShadowListUtilsPollData("A question", options, "two")
    host(list)
    assertEquals("A question", (list.headerView as ShadowListUtilsListHeader).title)
    val totalFooter = list.footerView as ShadowListUtilsPollFooter
    val totalLabel = (0 until totalFooter.childCount).map(totalFooter::getChildAt).filterIsInstance<ShadowListUtilsListFooter>().single()
    assertEquals("Total 8.0", totalLabel.text)
    assertEquals("Window seat has 13 percent", requireNotNull(list.rowView(0)).contentDescription)
    val info = AccessibilityNodeInfo.obtain()
    requireNotNull(list.rowView(1)).onInitializeAccessibilityNodeInfo(info)
    assertTrue(info.isChecked)
    assertFalse(info.isClickable)
    val header = View(testContext)
    val footer = View(testContext)
    list.headerView = header
    list.footerView = footer
    list.poll = ShadowListUtilsPollData("Changed")
    assertSame(header, list.headerView)
    assertSame(footer, list.footerView)
    assertTrue(list.rows.isEmpty())
  }

  @Test
  fun clearing_the_default_header_and_footer_keeps_them_absent() = onMain {
    val list = ShadowListUtilsPollList(testContext, theme)
    list.poll = ShadowListUtilsPollData("A question", options)
    list.headerView = null
    list.footerView = null
    list.poll = ShadowListUtilsPollData("Changed", options, "one")
    list.labels = ShadowListUtilsPollLabels()
    assertNull(list.headerView)
    assertNull(list.footerView)
  }

  @Test
  fun a_right_to_left_poll_mirrors_its_frames_and_long_labels_wrap() = onMain {
    val row = ShadowListUtilsPollRow(options[0], 8.0, theme = theme)
    val regular = row.layout(TEST_WIDTH_DP.px) as ShadowListUtilsPollRowLayout
    val mirrored = withRightToLeft { row.layout(TEST_WIDTH_DP.px) as ShadowListUtilsPollRowLayout }
    assertEquals(regular.barFillFrame.left, TEST_WIDTH_DP.px - mirrored.barFillFrame.right)
    val long = ShadowListUtilsPollOption("long", "A longer option ".repeat(10), 1.0)
    val large = withTextSize(1.6f) { ShadowListUtilsPollRow(long, 8.0, theme = theme).layout(TEST_WIDTH_DP.px) as ShadowListUtilsPollRowLayout }
    assertTrue(large.heightPx > regular.heightPx)
    assertTrue(large.label.lineCount > 1)
  }

  @Test
  fun a_poll_row_centers_its_body_against_the_selection() = onMain {
    val layout = ShadowListUtilsPollRow(options[0], 8.0, theme = theme).layout(TEST_WIDTH_DP.px) as ShadowListUtilsPollRowLayout
    assertEquals(72f.px, layout.heightPx)
    assertEquals(14f.pxf, layout.selectionFrame.top.toFloat(), 1f)
    assertEquals((layout.heightPx - layout.selectionFrame.bottom).toFloat(), layout.selectionFrame.top.toFloat(), 1f)
    assertEquals(layout.selectionFrame.centerY().toFloat(), (layout.labelFrame.top + layout.barFrame.bottom) / 2f, 1f)
  }
}
