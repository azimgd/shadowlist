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
  private var expanded = setOf("trips", "boarding-passes", "flights", "hotels")

  override fun load() {
    list.expandedKeys = expanded
    list.onExpandedChange = { keys -> expanded = keys; list.expandedKeys = keys }
    list.onPressItem = { item -> AlertDialog.Builder(context).setTitle(item.name).setMessage(item.key).setPositiveButton("Close", null).show() }
    val folders = listOf("trips", "boarding-passes", "flights", "hotels", "photos", "receipts")
    list.items = folders.map { folder ->
      ShadowListUtilsTreeNode(folder, folder, listOf(
        ShadowListUtilsTreeNode("$folder/trips", "trips", List(6) { ShadowListUtilsTreeNode("$folder/trips/$it", "trip-$it.pdf") }),
        ShadowListUtilsTreeNode("$folder/boarding-passes", "boarding-passes", List(6) { ShadowListUtilsTreeNode("$folder/passes/$it", "pass-$it.pdf") }),
        ShadowListUtilsTreeNode("$folder/0", "trips-0.pdf"),
        ShadowListUtilsTreeNode("$folder/1", "boarding-passes-1.ics"),
        ShadowListUtilsTreeNode("$folder/2", "flights-2.jpg"),
        ShadowListUtilsTreeNode("$folder/3", "hotels-3.json")))
    }
  }
}
