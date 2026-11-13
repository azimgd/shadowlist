package com.shadowlist.utils

import android.content.Context
import android.view.View

/*
 * A poll with its question and total. The app owns votes and selectedKey; onPressItem requests
 * a vote and setPoll shows the app's answer. Custom headerView and footerView are kept.
 */
class ShadowListUtilsPollList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  var poll: ShadowListUtilsPollData
    get() = storedPoll
    set(value) = setPoll(value, false)

  fun setPoll(poll: ShadowListUtilsPollData, animated: Boolean) {
    storedPoll = poll
    rebuildRows(animated)
  }

  var onPressItem: ((ShadowListUtilsPollOption) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
      rebuildRows()
    }

  var rowForItem: ((ShadowListUtilsPollOption) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels = ShadowListUtilsPollLabels()
    set(value) {
      field = value
      reloadRows()
    }

  private var storedPoll = ShadowListUtilsPollData()
  private var pollHeader: View? = null
  private var pollFooter: View? = null
  private val tally = DoubleArray(ShadowListUtilsCore.POLL_OUT_SLOTS)

  init {
    allowsSelection = false
    stickyHeader = true
    stickyFooter = true
    pollHeader = ShadowListUtilsListHeader(context, storedPoll.question, theme = theme)
    pollFooter = ShadowListUtilsListFooter(context, labels.totalVotes(0.0), theme)
    headerView = pollHeader
    footerView = pollFooter
    rebuildRows()
  }

  override fun makeRows(): List<ShadowListUtilsRow> {
    ShadowListUtilsCore.pollTally(storedPoll.options.map { it.votes }.toDoubleArray(), tally)
    val totalVotes = tally[ShadowListUtilsCore.POLL_OUT_TOTAL_VOTES]
    val leadingIndex = tally[ShadowListUtilsCore.POLL_OUT_LEADING_INDEX].toInt()
    if (headerView === pollHeader) {
      pollHeader = ShadowListUtilsListHeader(context, storedPoll.question, theme = theme)
      headerView = pollHeader
    }
    if (footerView === pollFooter) {
      pollFooter = ShadowListUtilsListFooter(context, labels.totalVotes(totalVotes), theme)
      footerView = pollFooter
    }
    return storedPoll.options.mapIndexed { index, item ->
      return@mapIndexed rowForItem?.invoke(item) ?: ShadowListUtilsPollRow(
        item, totalVotes, index == leadingIndex, item.key == storedPoll.selectedKey, onPressItem != null, theme, labels)
    }
  }

  override fun didSelectItem(index: Int) {
    storedPoll.options.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }
}
