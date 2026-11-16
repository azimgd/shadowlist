package com.shadowlist.utils

import android.content.Context
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputConnectionWrapper
import android.widget.EditText

/*
 * Finish native composition before applying a deferred application-controlled value.
 */
internal class ShadowListUtilsTextInputEditText(context: Context) : EditText(context) {
  override fun setTextSize(unit: Int, size: Float) {
    super.setTextSize(android.util.TypedValue.COMPLEX_UNIT_PX,
      kotlin.math.ceil(android.util.TypedValue.applyDimension(unit, size, resources.displayMetrics)))
    paint.isSubpixelText = true
  }

  var editingOwner: ShadowListUtilsTextInput? = null
  override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection? {
    val connection = super.onCreateInputConnection(outAttrs) ?: return null
    return object : InputConnectionWrapper(connection, false) {
      override fun finishComposingText(): Boolean {
        val handled = super.finishComposingText()
        editingOwner?.commitComposition()
        return handled
      }
    }
  }
}
