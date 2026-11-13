package com.shadowlist.utils

import android.view.accessibility.AccessibilityNodeInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TreeTest {
  private val items = listOf(ShadowListUtilsTreeNode("root", "Sources", listOf(
    ShadowListUtilsTreeNode("branch", "Views", listOf(ShadowListUtilsTreeNode("🦊", "Fox.kt"))),
    ShadowListUtilsTreeNode("leaf", "Main.kt"))), ShadowListUtilsTreeNode("empty", "Empty", emptyList()))

  @Test fun expansion_and_hidden_branches_use_shared_rows() = onMain {
    val list = ShadowListUtilsTreeList(testContext, testTheme)
    list.items = items
    assertEquals(listOf("root", "empty"), list.rows.map { it.key })
    list.toggle("root")
    list.toggle("branch")
    assertEquals(listOf("root", "branch", "🦊", "leaf", "empty"), list.rows.map { it.key })
    assertEquals(2, (list.rows[2] as ShadowListUtilsTreeRow).depth)
    list.toggle("root")
    assertEquals(listOf("root", "empty"), list.rows.map { it.key })
    list.toggle("root")
    assertEquals(listOf("root", "branch", "🦊", "leaf", "empty"), list.rows.map { it.key })
  }

  @Test fun controlled_expansion_waits_for_the_application() = onMain {
    val list = ShadowListUtilsTreeList(testContext, testTheme)
    list.items = items
    list.expandedKeys = emptySet()
    var requested = emptySet<String>()
    list.onExpandedChange = { requested = it }
    list.toggle("root")
    assertEquals(setOf("root"), requested)
    assertEquals(listOf("root", "empty"), list.rows.map { it.key })
    list.expandedKeys = requested
    assertEquals(listOf("root", "branch", "leaf", "empty"), list.rows.map { it.key })
    list.onExpandedChange = { list.expandedKeys = it }
    list.toggle("branch")
    assertEquals(listOf("root", "branch", "🦊", "leaf", "empty"), list.rows.map { it.key })
  }

  @Test fun initial_expansion_leaf_press_and_empty_folder() = onMain {
    val list = ShadowListUtilsTreeList(testContext, testTheme, setOf("root", "branch"))
    list.items = items
    assertEquals(listOf("root", "branch", "🦊", "leaf", "empty"), list.rows.map { it.key })
    val pressed = ArrayList<String>()
    list.onPressItem = { pressed.add(it.key) }
    list.didSelectItem(2)
    assertEquals(listOf("🦊"), pressed)
    list.toggle("empty")
    assertEquals(5, list.rows.size)
    list.items = items
    assertEquals(5, list.rows.size)
  }

  @Test fun custom_rows_receive_native_tree_metadata_and_toggle() = onMain {
    val list = ShadowListUtilsTreeList(testContext, testTheme)
    list.items = items
    var root: ShadowListUtilsTreeListRenderItemInfo? = null
    list.rowForItem = { info ->
      if (info.item.key == "root") root = info
      ShadowListUtilsTreeRow(info.item, info.depth, info.hasChildren, info.isExpanded, theme = testTheme)
    }
    assertTrue(root?.hasChildren ?: false)
    root?.toggle?.invoke()
    assertEquals(4, list.rows.size)
    assertEquals(0f, root!!.indent, 0f)
  }

  @Test fun controlled_requests_compose_and_uncontrolled_state_remains_separate() = onMain {
    val list = ShadowListUtilsTreeList(testContext, testTheme, setOf("root"))
    list.items = items
    list.expandedKeys = emptySet()
    val requests = ArrayList<Set<String>>()
    list.onExpandedChange = { requests.add(it) }
    list.toggle("root")
    list.toggle("root")
    assertEquals(listOf(setOf("root"), emptySet<String>()), requests)
    assertEquals(listOf("root", "empty"), list.rows.map { it.key })
    list.expandedKeys = null
    assertEquals(listOf("root", "branch", "leaf", "empty"), list.rows.map { it.key })
  }

  @Test fun expansion_intent_survives_data_arriving_later() = onMain {
    val list = ShadowListUtilsTreeList(testContext, testTheme, setOf("future"))
    list.items = items
    list.items = items + ShadowListUtilsTreeNode("future", "Loaded later", listOf(ShadowListUtilsTreeNode("later", "Later.txt")))
    assertEquals(listOf("future", "later"), list.rows.takeLast(2).map { it.key })
  }

  @Test fun row_accessibility_and_large_text_layout() = onMain {
    val row = ShadowListUtilsTreeRow(items[0], 1, true, true, interactive = true, theme = testTheme,
      labels = ShadowListUtilsTreeLabels("Open", "Close"))
    val layout = row.layout(240f.px)
    assertTrue(layout.heightPx >= 44f.px)
    val view = ShadowListUtilsTreeRowView(testContext)
    view.configure(row, layout)
    assertEquals("Sources", view.contentDescription)
    val info = AccessibilityNodeInfo.obtain()
    view.onInitializeAccessibilityNodeInfo(info)
    assertTrue(info.isClickable)
    assertTrue(info.actionList.any { it.label == "Close" })
    info.recycle()
  }
}
