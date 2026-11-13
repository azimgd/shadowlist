package com.shadowlist.utils

import android.content.Context
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.Button
import android.widget.HorizontalScrollView
import android.widget.LinearLayout
import android.widget.TextView

/*
 * The existing multiline input owns text editing. Assistant controls add attachments, model,
 * thinking, dictation and Stop without disabling the draft while a reply streams.
 */
class ShadowListUtilsAssistantComposer(
  context: Context,
  val theme: ShadowListUtilsTheme = ShadowListUtilsTheme.system(context),
) : LinearLayout(context) {
  var onSend: ((String, List<ShadowListUtilsAssistantAttachment>) -> Unit)? = null
    set(value) { field = value; updateControls() }
  var value: String? = null
    set(value) { field = value; input.value = value; updateControls() }
  var onChangeText: ((String) -> Unit)? = null
  var streaming = false
    set(value) { field = value; updateControls() }
  var sendWhileStreaming = false
    set(value) { field = value; updateControls() }
  var onStop: (() -> Unit)? = null
    set(value) { field = value; updateControls() }
  var attachments = emptyList<ShadowListUtilsAssistantAttachment>()
    set(value) { field = value; rebuildAttachments(); updateControls() }
  var onPressAttach: (() -> Unit)? = null
    set(value) { field = value; updateControls() }
  var onRemoveAttachment: ((String) -> Unit)? = null
    set(value) { field = value; rebuildAttachments() }
  var model: String? = null
    set(value) { field = value; updateControls() }
  var onPressModel: (() -> Unit)? = null
    set(value) { field = value; updateControls() }
  var thinking = false
    set(value) { field = value; updateControls() }
  var onThinkingChange: ((Boolean) -> Unit)? = null
    set(value) { field = value; updateControls() }
  var accessory: View? = null
    set(value) { field?.let(accessoryHost::removeView); field = value; value?.let(accessoryHost::addView) }
  var dictating = false
    set(value) { field = value; updateControls() }
  var onPressDictate: (() -> Unit)? = null
    set(value) { field = value; updateControls() }
  var editing = false
    set(value) { field = value; updateControls() }
  var onCancelEdit: (() -> Unit)? = null
  var maxLength = 4000
    set(value) { field = value; input.maxLength = value }
  var labels = ShadowListUtilsAssistantLabels()
    set(value) { field = value; rebuildAttachments(); updateControls() }

  internal val input = ShadowListUtilsChatInput(context, theme = theme)
  internal val primaryAction = Button(context)
  private val editingBanner = LinearLayout(context)
  private val editingLabel = TextView(context)
  private val cancelEdit = Button(context)
  private val attachmentScroll = HorizontalScrollView(context).apply { isHorizontalScrollBarEnabled = false }
  private val attachmentRow = LinearLayout(context)
  private val toolbar = LinearLayout(context)
  private val attach = Button(context)
  private val modelButton = Button(context)
  private val thinkingButton = Button(context)
  private val dictate = Button(context)
  private val accessoryHost = android.widget.FrameLayout(context)

  init {
    orientation = VERTICAL
    setBackgroundColor(theme.colors.background)
    editingBanner.addView(editingLabel, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f))
    editingBanner.addView(cancelEdit)
    addView(editingBanner)
    attachmentScroll.addView(attachmentRow)
    addView(attachmentScroll)
    input.maxLength = maxLength
    input.send.visibility = GONE
    input.onChangeText = { onChangeText?.invoke(it) }
    input.input.addTextChangedListener(object : TextWatcher {
      override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {}
      override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) {}
      override fun afterTextChanged(text: Editable?) { updateControls() }
    })
    addView(input, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    toolbar.gravity = android.view.Gravity.CENTER_VERTICAL
    toolbar.setPadding(theme.spacing.sm.px, 0, theme.spacing.sm.px, theme.spacing.sm.px)
    for (button in listOf(attach, modelButton, thinkingButton, dictate, primaryAction, cancelEdit)) {
      button.isAllCaps = false
      button.minWidth = 0; button.minimumWidth = 0
      button.minHeight = 32f.px; button.minimumHeight = 32f.px
      button.setPadding(8f.px, 0, 8f.px, 0)
      button.setTextColor(theme.colors.label)
    }
    toolbar.addView(attach, LayoutParams(32f.px, 32f.px))
    toolbar.addView(modelButton)
    toolbar.addView(thinkingButton)
    toolbar.addView(accessoryHost, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f))
    toolbar.addView(dictate, LayoutParams(32f.px, 32f.px))
    toolbar.addView(primaryAction, LayoutParams(40f.px, 36f.px))
    addView(toolbar)
    attach.setOnClickListener { onPressAttach?.invoke() }
    modelButton.setOnClickListener { onPressModel?.invoke() }
    thinkingButton.setOnClickListener { onThinkingChange?.invoke(!thinking) }
    dictate.setOnClickListener { onPressDictate?.invoke() }
    cancelEdit.setOnClickListener { onCancelEdit?.invoke() }
    primaryAction.setOnClickListener { sendOrStop() }
    updateControls()
  }

  fun setDraft(text: String, focus: Boolean = true) {
    input.input.setText(text)
    input.input.setSelection(input.input.text.length)
    if (focus) this.focus()
  }
  fun getDraft(): String = input.input.text.toString()
  fun clearDraft() { setDraft("", focus = false) }
  fun focus() {
    input.input.requestFocus()
    (context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager)?.showSoftInput(input.input, InputMethodManager.SHOW_IMPLICIT)
  }

  private fun rebuildAttachments() {
    attachmentRow.removeAllViews()
    for (attachment in attachments) attachmentRow.addView(ShadowListUtilsAssistantAttachmentChip(context, attachment, theme).apply {
      labels = this@ShadowListUtilsAssistantComposer.labels
      onRemove = onRemoveAttachment
    })
    attachmentScroll.visibility = if (attachments.isEmpty()) GONE else VISIBLE
  }

  private fun updateControls() {
    val draft = ShadowListUtilsCore.trim(getDraft()).isNotEmpty() || attachments.isNotEmpty()
    val stop = streaming && !(sendWhileStreaming && draft)
    input.labels = ShadowListUtilsChatLabels(placeholder = labels.placeholder, send = labels.send)
    input.input.contentDescription = labels.placeholder
    editingBanner.visibility = if (editing) VISIBLE else GONE
    editingLabel.text = labels.editingMessage
    cancelEdit.text = "×"; cancelEdit.contentDescription = labels.cancelEditing
    attach.text = "+"; attach.contentDescription = labels.addAttachment
    attach.visibility = if (onPressAttach != null) VISIBLE else GONE
    modelButton.text = model.orEmpty(); modelButton.contentDescription = model?.let(labels.model)
    modelButton.visibility = if (model != null) VISIBLE else GONE
    modelButton.isEnabled = onPressModel != null
    thinkingButton.text = labels.thinkingToggle
    thinkingButton.contentDescription = labels.thinkingToggleDescription
    thinkingButton.isSelected = thinking
    thinkingButton.visibility = if (onThinkingChange != null) VISIBLE else GONE
    dictate.text = if (dictating) "■" else "●"
    dictate.contentDescription = if (dictating) labels.stopDictating else labels.dictate
    dictate.visibility = if (onPressDictate != null) VISIBLE else GONE
    primaryAction.text = if (stop) "■" else "↑"
    primaryAction.contentDescription = if (stop) labels.stop else labels.send
    primaryAction.isEnabled = if (stop) onStop != null else draft && (!streaming || sendWhileStreaming) && onSend != null
  }

  internal fun sendOrStop() {
    val text = ShadowListUtilsCore.trim(getDraft())
    val draft = text.isNotEmpty() || attachments.isNotEmpty()
    if (streaming && !(sendWhileStreaming && draft)) { onStop?.invoke(); return }
    val send = onSend ?: return
    if (!draft || streaming && !sendWhileStreaming) return
    send(text, attachments)
    clearDraft()
  }
}
