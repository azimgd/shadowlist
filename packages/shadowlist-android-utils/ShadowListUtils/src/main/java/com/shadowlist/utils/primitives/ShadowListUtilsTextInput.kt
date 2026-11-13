package com.shadowlist.utils

import android.text.Editable
import android.text.InputFilter
import android.text.TextWatcher
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.EditText

/* One native editing owner for controlled fields and the existing uncontrolled chat draft. */
internal class ShadowListUtilsTextInput(private val input: EditText, defaultValue: String = "", initialMaxLength: Int = Int.MAX_VALUE) : TextWatcher {
  var value: String? = null
    set(value) { field = value; show(value ?: draft) }
  var onChangeText: ((String) -> Unit)? = null
  var onContentSizeChange: (() -> Unit)? = null
  var maxLength = initialMaxLength
    set(value) { field = value; input.filters = arrayOf(InputFilter.LengthFilter(value.coerceAtLeast(0))) }
  private var draft = defaultValue
  private var updating = false
  init {
    input.filters = arrayOf(InputFilter.LengthFilter(initialMaxLength.coerceAtLeast(0)))
    input.setText(defaultValue)
    input.addTextChangedListener(this)
    (input as? ShadowListUtilsTextInputEditText)?.editingOwner = this
  }
  val text: String get() = input.text.toString()
  private val composing: Boolean get() = BaseInputConnection.getComposingSpanStart(input.text) >= 0
  private fun show(text: String, notifySize: Boolean = true) {
    if (composing || input.text.toString() == text) return
    val start = input.selectionStart.coerceAtLeast(0)
    val end = input.selectionEnd.coerceAtLeast(start)
    updating = true
    input.setText(text)
    input.setSelection(start.coerceAtMost(input.length()), end.coerceAtMost(input.length()))
    updating = false
    if (notifySize) onContentSizeChange?.invoke()
  }
  /* Single-line RN fields submit and blur; multiline text keeps the native newline path. */
  fun configureSubmit(multiline: Boolean, onSubmit: (() -> Unit)? = null) {
    input.setOnEditorActionListener { _, action, _ ->
      if ((action and EditorInfo.IME_MASK_ACTION) == 0 && action != EditorInfo.IME_NULL) {
        true
      } else if (multiline) {
        action == EditorInfo.IME_ACTION_NEXT || action == EditorInfo.IME_ACTION_PREVIOUS
      } else {
        val windowToken = input.windowToken
        val keyboard = input.context.getSystemService(InputMethodManager::class.java)
        input.clearFocus()
        if (windowToken != null) keyboard?.hideSoftInputFromWindow(windowToken, 0)
        onSubmit?.invoke()
        true
      }
    }
  }
  /* Keyboard and font changes can make TextView rewrite text; they are not user edits. */
  fun configure(update: () -> Unit) {
    val start = input.selectionStart.coerceAtLeast(0)
    val end = input.selectionEnd.coerceAtLeast(start)
    val wasUpdating = updating
    updating = true
    try { update() } finally { updating = wasUpdating }
    show(value ?: draft, notifySize = false)
    input.setSelection(start.coerceAtMost(input.length()), end.coerceAtMost(input.length()))
  }
  fun clearDraft() { draft = ""; show(value ?: draft) }
  fun commitComposition() { value?.let { show(it) } }
  override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {}
  override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) {}
  override fun afterTextChanged(text: Editable?) {
    if (updating) return
    val next = text.toString()
    if (value == null) draft = next
    onChangeText?.invoke(next)
    value?.let { show(it, notifySize = false) }
    onContentSizeChange?.invoke()
  }
}
