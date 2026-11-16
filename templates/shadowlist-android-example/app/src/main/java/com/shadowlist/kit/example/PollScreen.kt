package com.shadowlist.kit.example

import android.content.Context
import android.view.View
import com.shadowlist.utils.ShadowListUtilsPollData
import com.shadowlist.utils.ShadowListUtilsPollList
import com.shadowlist.utils.ShadowListUtilsPollOption

/*
 * The application applies each choice and sends the updated model back to the list.
 */
class PollScreen(context: Context) : Screen {
  val list = ShadowListUtilsPollList(context)
  private var poll = ShadowListUtilsPollData("Where should we travel next?", listOf(
    ShadowListUtilsPollOption("lisbon", "Lisbon", 12.0),
    ShadowListUtilsPollOption("kyoto", "Kyoto", 9.0),
    ShadowListUtilsPollOption("reykjavik", "Reykjavík", 7.0),
    ShadowListUtilsPollOption("oaxaca", "Oaxaca", 5.0),
  ))
  override val view: View get() = list

  override fun load() {
    list.setOnApplyWindowInsetsListener { _, insets ->
      @Suppress("DEPRECATION")
      list.setPadding(0, 0, 0, insets.systemWindowInsetBottom)
      insets
    }
    list.onPressItem = ::choose
    list.poll = poll
  }

  private fun choose(option: ShadowListUtilsPollOption) {
    if (poll.selectedKey == option.key) return
    poll = poll.copy(selectedKey = option.key, options = poll.options.map {
      when (it.key) {
        poll.selectedKey -> it.copy(votes = it.votes - 1)
        option.key -> it.copy(votes = it.votes + 1)
        else -> it
      }
    })
    list.setPoll(poll, animated = true)
  }
}
