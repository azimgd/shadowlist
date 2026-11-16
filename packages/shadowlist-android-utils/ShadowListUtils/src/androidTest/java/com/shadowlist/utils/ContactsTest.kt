package com.shadowlist.utils

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.drawable.ColorDrawable
import android.view.View
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
import androidx.core.graphics.ColorUtils
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.shadowlist.kit.ShadowListKitListView
import com.shadowlist.kit.ShadowListKitSwipeAction
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ContactsTest {
  private val theme = testTheme
  private val width = TEST_WIDTH_DP.px

  /*
   * Where a row's text starts: after the inset, the avatar and the gap.
   */
  private val textLeft = 16f.px + 40f.px + 12f.px

  private fun named(names: List<String>): List<ShadowListUtilsContactItem> =
    names.mapIndexed { index, name -> ShadowListUtilsContactItem(key = "contact-$index", name = name) }

  private fun contactRow(list: ShadowListUtilsList, index: Int): ShadowListUtilsContactRow? =
    list.rowView(index)?.row as? ShadowListUtilsContactRow

  private fun layout(row: ShadowListUtilsContactRow): ShadowListUtilsContactRowLayout =
    row.layout(width) as ShadowListUtilsContactRowLayout

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

  /*
   * The section headers a list shows.
   */
  private fun shownHeaders(list: ShadowListUtilsList): List<ShadowListUtilsSectionHeaderView> =
    (0 until list.childCount).map(list::getChildAt).filterIsInstance<ShadowListUtilsSectionHeaderView>()
      .filter { it.visibility == View.VISIBLE }

  /*
   * The box around what a chevron of 20 draws, in pixels.
   */
  private fun chevronBounds(rows: IntRange = 0 until 20f.px): Rect {
    val size = 20f.px
    val icon = ShadowListUtilsIconView(testContext, ShadowListUtilsIconView.Kind.CHEVRON)
    icon.color = theme.colors.label
    icon.place(0, 0, size, size)
    val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
    icon.draw(Canvas(bitmap))
    val bounds = Rect()
    for (y in rows) {
      for (x in 0 until size) {
        if (bitmap.getPixel(x, y) ushr 24 != 0) bounds.union(Rect(x, y, x + 1, y + 1))
      }
    }
    return bounds
  }

  /*
   * The same for the middle row of the box alone, where the chevron's point is.
   */
  private fun chevronPoint(): Rect = chevronBounds(20f.px / 2..20f.px / 2)

  @Test
  fun the_list_shows_its_items() = onMain {
    val list = ShadowListUtilsContactsList(testContext, theme)
    host(list)
    list.items = Fixture.contacts(5)
    host(list)
    assertEquals(List(5) { "contact-$it" }, list.keys)
    assertTrue(list.rowView(0) is ShadowListUtilsContactRowView)
    assertFalse(list.allowsSelection)
    assertTrue(list.disclosureIndicator)
  }

  @Test
  fun a_changed_item_is_configured_again_and_a_deleted_item_leaves() = onMain {
    val list = ShadowListUtilsContactsList(testContext, theme)
    host(list)
    val items = Fixture.contacts(4).toMutableList()
    list.items = items.toList()
    host(list)
    val untouched = list.rowView(0)
    val untouchedRow = untouched?.row
    items[1] = items[1].copy(subtitle = "Moved to the aisle")
    list.setItems(items.toList(), false)
    host(list)
    assertEquals("Moved to the aisle", contactRow(list, 1)?.item?.subtitle)
    assertSame(untouched, list.rowView(0))
    assertSame(untouchedRow, list.rowView(0)?.row)
    items.removeAt(3)
    list.setItems(items.toList(), true)
    assertEquals(listOf("contact-0", "contact-1", "contact-2"), list.keys)
  }

  @Test
  fun on_press_item_runs_for_a_tapped_contact_and_shows_the_chevron() = onMain {
    val list = ShadowListUtilsContactsList(testContext, theme)
    host(list)
    list.items = Fixture.contacts(3)
    host(list)
    assertEquals(false, contactRow(list, 0)?.showsChevron)
    val pressed = ArrayList<String>()
    list.onPressItem = { pressed.add(it.key) }
    host(list)
    assertTrue(list.allowsSelection)
    assertEquals(true, contactRow(list, 0)?.showsChevron)
    assertEquals(Button::class.java.name, nodeInfo(requireNotNull(list.rowView(0))).className)
    list.tap(1)
    assertEquals(listOf("contact-1"), pressed)
    assertEquals(0, list.selectedIndices.size)
    list.disclosureIndicator = false
    host(list)
    assertEquals(false, contactRow(list, 0)?.showsChevron)
  }

  @Test
  fun on_delete_adds_one_destructive_trailing_action_in_the_themes_red() = onMain {
    val list = ShadowListUtilsContactsList(testContext, theme)
    host(list)
    list.items = Fixture.contacts(3)
    host(list)
    assertTrue(list.trailingActions(1).isEmpty())
    assertTrue(list.leadingActions(1).isEmpty())
    val deleted = ArrayList<String>()
    list.onDelete = { deleted.add(it.key) }
    val actions = list.trailingActions(1)
    assertEquals(listOf("Delete"), actions.map { it.title })
    assertEquals(ShadowListKitSwipeAction.Style.DESTRUCTIVE, actions.first().style)
    assertEquals(theme.colors.red, actions.first().backgroundColor)
    actions.first().perform()
    assertEquals(listOf("contact-1"), deleted)
    assertTrue(list.leadingActions(1).isEmpty())
    val view = requireNotNull(list.rowView(2)) { "item 2 is not mounted" }
    val reader = customActions(view)
    assertEquals(listOf("Delete"), reader.map { it.label.toString() })
    assertTrue(view.performAccessibilityAction(reader[0].id, null))
    assertEquals(listOf("contact-1", "contact-2"), deleted)
  }

  @Test
  fun the_row_layout_holds_its_metrics() = onMain {
    val typography = theme.typography
    val layout = layout(ShadowListUtilsContactRow(Fixture.contact(0), true, theme))
    val height = 24f.px + typography.body.scaledLineHeightPx() + 1f.px + typography.subhead.scaledLineHeightPx()
    assertEquals(height, layout.heightPx)
    val avatarTop = (height - 40f.px) / 2
    assertEquals(Rect(16f.px, avatarTop, 16f.px + 40f.px, avatarTop + 40f.px), layout.avatarFrame)
    assertEquals(textLeft, layout.nameFrame.left)
    assertEquals(12f.px, layout.nameFrame.top)
    assertEquals(textLeft, layout.subtitleFrame.left)
    assertEquals(12f.px + typography.body.scaledLineHeightPx() + kotlin.math.floor(1f.pxf).toInt(), layout.subtitleFrame.top)
    val chevronTop = (height - 20f.px) / 2
    assertEquals(Rect(width - 12f.px - 20f.px, chevronTop, width - 12f.px, chevronTop + 20f.px), layout.chevronFrame)
    assertEquals(Rect(68f.px, height - ShadowListUtilsTheme.HAIRLINE_PX, width, height), layout.separatorFrame)
    assertEquals(1, layout.name.lineCount)
    assertLines(layout.name, typography.body, 1)
    assertLines(layout.subtitle, typography.subhead, 1)
    // The chevron draws through the middle of its box.
    val center = 20f.px / 2
    assertTrue(Rect.intersects(chevronBounds(), Rect(center - 1f.px, center - 1f.px, center + 1f.px, center + 1f.px)))

    val short = layout(ShadowListUtilsContactRow(Fixture.contact(1).copy(subtitle = null), false, theme))
    assertEquals(40f.px + 24f.px, short.heightPx)
    assertNull(short.subtitle)
    assertEquals(Rect(), short.chevronFrame)
    assertEquals(12f.px, short.avatarFrame.top)
    assertEquals(12f.px + (40f.px - typography.body.scaledLineHeightPx()) / 2, short.nameFrame.top)
  }

  @Test
  fun a_name_wraps_at_a_large_text_size() {
    val body = theme.typography.body
    val contact = Fixture.contact(0).copy(name = "Bartholomew ".repeat(6))
    val row = ShadowListUtilsContactRow(contact, false, theme)
    val plain = layout(row)
    assertEquals(1, plain.name.lineCount)
    assertEquals(24f.px + body.scaledLineHeightPx() + 1f.px + theme.typography.subhead.scaledLineHeightPx(), plain.heightPx)
    val large = withTextSize(1.5f) { layout(row) }
    assertTrue(large.name.lineCount > 1)
    assertEquals(large.nameFrame.bottom + kotlin.math.floor(1f.pxf).toInt(), large.subtitleFrame.top)
    assertEquals((large.subtitleFrame.bottom + 24f.px / 2).toFloat(), large.heightPx.toFloat(), 1f)
    withTextSize(1.5f) { assertLines(large.name, body) }
  }

  @Test
  fun a_right_to_left_row_mirrors_its_frames() = onMain {
    val row = ShadowListUtilsContactRow(Fixture.contact(0), true, theme)
    val plain = layout(row)
    val mirrored = withRightToLeft { layout(row) }
    assertEquals(width - plain.avatarFrame.right, mirrored.avatarFrame.left)
    assertEquals(width - textLeft, mirrored.nameFrame.right)
    assertEquals(12f.px, mirrored.chevronFrame.left)
    assertEquals(Rect(0, plain.separatorFrame.top, width - 68f.px, plain.separatorFrame.bottom), mirrored.separatorFrame)
    // The chevron of a layout that runs from right to left points left.
    val pointsRight = chevronBounds()
    val pointsLeft = withRightToLeft { chevronBounds() }
    assertEquals(20f.px - pointsRight.exactCenterX(), pointsLeft.exactCenterX(), 1f)
    assertTrue(chevronPoint().exactCenterX() > 20f.px / 2f)
    assertTrue(withRightToLeft { chevronPoint() }.exactCenterX() < 20f.px / 2f)
  }

  @Test
  fun a_pressed_row_highlights_in_the_fill_color_at_once() = onMain {
    val view = ShadowListUtilsContactRowView(testContext)
    view.configure(ShadowListUtilsContactRow(Fixture.contact(0), false, theme), null)
    assertEquals(theme.colors.background, backgroundColor(view))
    view.setHighlighted(true, true)
    // The fill color has an alpha and lies over the resting background.
    assertEquals(ColorUtils.compositeColors(theme.colors.fill, theme.colors.background), backgroundColor(view))
    view.setHighlighted(false, true)
    assertEquals(theme.colors.background, backgroundColor(view))
    assertEquals("Person 0, Seat 0", view.contentDescription)
  }

  // region Sections

  @Test
  fun group_into_sections_groups_by_the_first_letter_with_the_rest_last() {
    val sections = shadowListUtilsGroupIntoSections(named(listOf("bob", "Ada", "  Bea", "42", "", "alan", "Émile", "_x")))
    assertEquals(listOf("A", "B", "É", "#"), sections.map { it.title })
    assertEquals(sections.map { it.title }, sections.map { it.key })
    assertEquals(listOf("Ada", "alan"), sections[0].items.map { it.name })
    assertEquals(listOf("bob", "  Bea"), sections[1].items.map { it.name })
    assertEquals(listOf("42", "", "_x"), sections[3].items.map { it.name })
    assertTrue(shadowListUtilsGroupIntoSections(emptyList()).isEmpty())
  }

  @Test
  fun group_into_sections_titles_by_the_first_grapheme_cluster() {
    // A letter with a combining accent is one character, and so is a flag, which is no letter.
    val sections = shadowListUtilsGroupIntoSections(
      named(listOf("émile", "🇫🇷 France", "ßen", "﻿zoe")))
    assertEquals(listOf("É", "SS", "Z", "#"), sections.map { it.title })
    assertEquals(listOf("🇫🇷 France"), sections[3].items.map { it.name })
  }

  /*
   * group-into-sections.json is a list of cases: input.names in, output.sections out as
   * { title, names }.
   */
  @Test
  fun group_into_sections_matches_the_vectors() {
    for (entry in Vectors.cases("group-into-sections.json")) {
      val name = entry.getString("name")
      val input = entry.getJSONObject("input").getJSONArray("names")
      val expected = entry.getJSONObject("output").getJSONArray("sections")
      val sections = shadowListUtilsGroupIntoSections(named(List(input.length()) { input.getString(it) }))
      val expectedSections = List(expected.length()) { expected.getJSONObject(it) }
      assertEquals(name, expectedSections.map { it.getString("title") }, sections.map { it.title })
      assertEquals(
        name,
        expectedSections.map { section -> section.getJSONArray("names").let { names -> List(names.length()) { names.getString(it) } } },
        sections.map { section -> section.items.map { it.name } })
    }
  }

  @Test
  fun the_section_list_shows_sticky_headers_with_their_counts() = onMain {
    val list = ShadowListUtilsContactsSectionList(testContext, theme)
    assertTrue(list.stickySectionHeaders)
    host(list)
    list.sections = shadowListUtilsGroupIntoSections(named(listOf("Ada", "Alan", "Bea", "Cy", "Cleo", "Cora")))
    host(list)
    assertEquals(3, list.numberOfSections)
    assertEquals(listOf("Ada", "Alan", "Bea", "Cy", "Cleo", "Cora"), list.items.map { it.name })
    assertEquals(3, list.firstItemIndexInSection(2))
    assertEquals(0, list.sectionForItem(0))
    assertEquals(1, list.sectionForItem(2))
    val headerHeight = theme.typography.footnote.scaledLineHeightPx() + 20f.px
    assertEquals(RectF(0f, 0f, width.toFloat(), headerHeight.toFloat()), list.rectForHeaderInSection(0))
    assertEquals(headerHeight.toFloat(), list.rectForItem(0)?.top)
    assertEquals(listOf("A, 2", "B, 1", "C, 3"), shownHeaders(list).map { it.contentDescription.toString() }.sorted())
    assertTrue(list.rowView(0) is ShadowListUtilsContactRowView)
  }

  @Test
  fun setting_items_on_the_section_list_groups_them() = onMain {
    val list = ShadowListUtilsContactsSectionList(testContext, theme)
    host(list)
    list.items = named(listOf("Cy", "Ada", "Bea", "Alan"))
    host(list)
    assertEquals(listOf("A", "B", "C"), list.sections.map { it.title })
    assertEquals(listOf("Ada", "Alan", "Bea", "Cy"), list.items.map { it.name })
    assertEquals(listOf("contact-1", "contact-3", "contact-2", "contact-0"), list.keys)
  }

  @Test
  fun setting_sections_keeps_them_as_given() = onMain {
    val list = ShadowListUtilsContactsSectionList(testContext, theme)
    host(list)
    val contacts = named(listOf("Ada", "Bea", "Cy"))
    list.sections = listOf(
      ShadowListUtilsContactSection(key = "late", title = "Late", items = listOf(contacts[2])),
      ShadowListUtilsContactSection(key = "early", title = "Early", items = listOf(contacts[0], contacts[1])))
    host(list)
    assertEquals(listOf("late", "early"), list.sections.map { it.key })
    assertEquals(listOf("Cy", "Ada", "Bea"), list.items.map { it.name })
    assertEquals(2, list.numberOfSections)
    assertEquals(1, list.firstItemIndexInSection(1))
    assertNotNull(list.rowView(2))
  }

  @Test
  fun a_sections_header_follows_its_count() = onMain {
    val list = ShadowListUtilsContactsSectionList(testContext, theme)
    host(list)
    val contacts = named(listOf("Ada", "Alan", "Bea"))
    list.sections = shadowListUtilsGroupIntoSections(contacts)
    host(list)
    list.setSections(
      shadowListUtilsGroupIntoSections(contacts + ShadowListUtilsContactItem(key = "contact-9", name = "Abel")), false)
    host(list)
    assertEquals(listOf("A, 3", "B, 1"), shownHeaders(list).map { it.contentDescription.toString() }.sorted())
  }

  @Test
  fun a_new_theme_reaches_the_section_headers() = onMain {
    val list = ShadowListUtilsContactsSectionList(testContext, ShadowListUtilsTheme.LIGHT)
    host(list)
    list.sections = shadowListUtilsGroupIntoSections(named(listOf("Ada", "Bea")))
    host(list)
    list.theme = ShadowListUtilsTheme.DARK
    host(list)
    val headers = shownHeaders(list)
    assertEquals(2, headers.size)
    for (header in headers) {
      assertEquals(ShadowListUtilsTheme.DARK, (header.row as? ShadowListUtilsSectionHeader)?.theme)
      assertEquals(ShadowListUtilsTheme.DARK.colors.elevated, backgroundColor(header))
    }
    assertEquals(ShadowListUtilsTheme.DARK, contactRow(list, 0)?.theme)
  }

  @Test
  fun the_section_lists_closures_and_index_titles() = onMain {
    val list = ShadowListUtilsContactsSectionList(testContext, theme)
    host(list)
    list.sections = shadowListUtilsGroupIntoSections(named(listOf("Ada", "Bea", "Cy")))
    val pressed = ArrayList<String>()
    val deleted = ArrayList<String>()
    list.onPressItem = { pressed.add(it.name) }
    list.onDelete = { deleted.add(it.name) }
    host(list)
    list.tap(1)
    list.trailingActions(2).first().perform()
    assertEquals(listOf("Bea"), pressed)
    assertEquals(listOf("Cy"), deleted)
    val sections = list.dataSource as ShadowListKitListView.Sections
    assertNull(sections.sectionIndexTitles(list))
    list.sectionIndexTitles = listOf("A", "B", "C")
    assertEquals(listOf("A", "B", "C"), sections.sectionIndexTitles(list))
  }

  // endregion
}
