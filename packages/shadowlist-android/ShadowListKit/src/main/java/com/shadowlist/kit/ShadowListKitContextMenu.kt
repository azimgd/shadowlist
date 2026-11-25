package com.shadowlist.kit

import android.view.HapticFeedbackConstants
import android.view.Menu
import android.view.View
import android.widget.PopupMenu

/* Native menu resolution shared by a list cell and a standalone action surface. */
internal class ShadowListKitContextMenu {
  private var popup: PopupMenu? = null
  private var anchor: View? = null

  fun show(
    anchor: View,
    configure: (Menu) -> Boolean,
    isValid: () -> Boolean,
    onWillShow: (() -> Unit)? = null
  ): Boolean {
    if (!isValid()) return false
    val next = PopupMenu(anchor.context, anchor)
    if (!configure(next.menu) || next.menu.size() == 0 || !isValid()) return false
    onWillShow?.invoke()
    if (!isValid()) return false
    close()
    popup = next
    this.anchor = anchor
    next.setOnDismissListener {
      if (popup === next) {
        popup = null
        this.anchor = null
      }
    }
    anchor.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    next.show()
    return true
  }

  fun close() {
    val previous = popup
    popup = null
    anchor = null
    previous?.dismiss()
  }

  fun cellWillRecycle(cell: View) {
    if (anchor === cell) close()
  }
}
