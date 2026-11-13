package com.shadowlist.kit.example

import android.app.AlertDialog
import android.content.Context
import android.view.View
import com.shadowlist.utils.ShadowListUtilsTreeList
import com.shadowlist.utils.ShadowListUtilsTreeNode

/*
 * The app controls expansion; the list owns only the visible rows of this file tree.
 */
class TreeScreen(private val context: Context) : Screen {
  val list = ShadowListUtilsTreeList(context)
  override val view: View get() = list
  private var expanded = setOf("src")

  override fun load() {
    list.expandedKeys = expanded
    list.onExpandedChange = { keys -> expanded = keys; list.expandedKeys = keys }
    list.onPressItem = { item -> AlertDialog.Builder(context).setTitle(item.name).setMessage(item.key).setPositiveButton("Close", null).show() }
    list.items = listOf(
      ShadowListUtilsTreeNode("src", "src", listOf(
        ShadowListUtilsTreeNode("components", "components", listOf(
          ShadowListUtilsTreeNode("avatar", "Avatar.tsx"), ShadowListUtilsTreeNode("header", "Header.tsx"))),
        ShadowListUtilsTreeNode("screens", "screens", listOf(
          ShadowListUtilsTreeNode("feed", "FeedScreen.tsx"), ShadowListUtilsTreeNode("chat", "ChatScreen.tsx"))),
        ShadowListUtilsTreeNode("app", "App.tsx"))),
      ShadowListUtilsTreeNode("assets", "assets", emptyList()),
      ShadowListUtilsTreeNode("readme", "README.md"))
  }
}
