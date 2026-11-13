package com.shadowlist.kit.example

import android.content.Context
import android.view.View
import com.shadowlist.utils.ShadowListUtilsActivityHeader
import com.shadowlist.utils.ShadowListUtilsActivityHeaderAction
import com.shadowlist.utils.ShadowListUtilsActivityActor
import com.shadowlist.utils.ShadowListUtilsActivityItem
import com.shadowlist.utils.ShadowListUtilsActivityList

/*
 * The app owns read state. Press an unread activity to mark it read; refresh prepends a new one.
 */
class ActivityScreen(context: Context) : Screen {
  val list = ShadowListUtilsActivityList(context)
  private var items = List(100) { activity(it) }
  private var fresh = 100
  override val view: View get() = list

  override fun load() {
    list.headerView = ShadowListUtilsActivityHeader(list.context, "Activity", "Your trip crew", listOf(
      ShadowListUtilsActivityHeaderAction("Mark all read", {
        items = items.map { it.copy(read = true) }
        list.setItems(items, animated = true)
      }),
    ), list.context.listTheme)
    list.onPressItem = { item ->
      items = items.map { if (it.key == item.key) it.copy(read = true) else it }
      list.setItems(items, animated = true)
    }
    list.onRefresh = {
      items = listOf(activity(fresh++)) + items
      list.setItems(items, animated = true)
      list.refreshing = false
    }
    list.items = items
  }

  private fun activity(index: Int): ShadowListUtilsActivityItem {
    val actions = listOf("started following you", "liked your trip photo", "tagged you in a check-in",
      "added you to a trip crew", "commented on your itinerary", "is boarding at gate 22")
    return ShadowListUtilsActivityItem(
      key = "activity-$index",
      actor = ShadowListUtilsActivityActor(FixtureStrings.characterNames[index % FixtureStrings.characterNames.size]),
      action = actions[index % actions.size],
      text = FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.size].take(90),
      createdAtMs = System.currentTimeMillis().toDouble() - (index % 59 + 1) * 60000.0,
      read = index % 3 == 0,
    )
  }
}
