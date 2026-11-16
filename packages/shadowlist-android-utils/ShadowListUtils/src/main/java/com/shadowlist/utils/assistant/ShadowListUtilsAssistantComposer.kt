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
import kotlin.math.ceil
import kotlin.math.floor

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
  private val field = LinearLayout(context).apply { orientation = VERTICAL }
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
    background = android.graphics.drawable.LayerDrawable(arrayOf(
      android.graphics.drawable.ColorDrawable(theme.colors.background),
      android.graphics.drawable.ColorDrawable(theme.colors.separator),
    )).apply { setLayerHeight(1, ShadowListUtilsTheme.HAIRLINE_PX); setLayerGravity(1, android.view.Gravity.TOP) }
    setPadding(theme.spacing.sm.px, theme.spacing.sm.px + ShadowListUtilsTheme.HAIRLINE_PX, theme.spacing.sm.px, theme.spacing.sm.px)
    setOnApplyWindowInsetsListener { _, insets ->
      @Suppress("DEPRECATION")
      setPadding(paddingLeft, paddingTop, paddingRight, insets.systemWindowInsetBottom.takeIf { it > 0 } ?: theme.spacing.sm.px)
      insets
    }
    field.background = android.graphics.drawable.GradientDrawable().apply {
      setColor(theme.colors.elevated); cornerRadius = theme.radius.xl.pxf; setStroke(ShadowListUtilsTheme.HAIRLINE_PX, theme.colors.separator)
    }
    field.setPadding(ShadowListUtilsTheme.HAIRLINE_PX, theme.spacing.sm.px + ShadowListUtilsTheme.HAIRLINE_PX, ShadowListUtilsTheme.HAIRLINE_PX, ShadowListUtilsTheme.HAIRLINE_PX)
    editingBanner.addView(editingLabel, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f))
    editingBanner.addView(cancelEdit)
    addView(editingBanner)
    addView(field, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    attachmentScroll.addView(attachmentRow)
    field.addView(attachmentScroll)
    input.maxLength = maxLength
    input.send.visibility = GONE
    input.useEmbeddedAppearance()
    input.input.maxHeight = 140f.px
    input.onChangeText = { onChangeText?.invoke(it) }
    input.input.addTextChangedListener(object : TextWatcher {
      override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {}
      override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) {}
      override fun afterTextChanged(text: Editable?) { updateControls() }
    })
    field.addView(input, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    toolbar.gravity = android.view.Gravity.CENTER_VERTICAL
    /*
     * React Native rounds the field's height once. The toolbar's bottom padding takes what the
     * top padding and the one-line input leave of that sum.
     */
    val toolbarPadding = theme.spacing.xs + 2
    val toolbarBottom = (theme.spacing.sm + ShadowListUtilsChatInput.INPUT_MIN_HEIGHT_DP + toolbarPadding).px -
      theme.spacing.sm.px - ShadowListUtilsChatInput.INPUT_MIN_HEIGHT_DP.px
    toolbar.setPadding(toolbarPadding.px, 0, toolbarPadding.px, toolbarBottom)
    for (button in listOf(attach, modelButton, thinkingButton, dictate, primaryAction, cancelEdit)) {
      button.isAllCaps = false
      button.stateListAnimator = null
      button.elevation = 0f
      button.textSize = 13f
      button.typeface = android.graphics.Typeface.create("sans-serif-medium", android.graphics.Typeface.NORMAL)
      button.includeFontPadding = false
      button.background = android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT)
      button.minWidth = 0; button.minimumWidth = 0
      button.minHeight = 0; button.minimumHeight = 0
      // React Native floors a text origin to the pixel grid.
      button.setPadding(floor(theme.spacing.md.pxf).toInt(), 0, ceil(theme.spacing.md.pxf).toInt(), 0)
      button.setTextColor(theme.colors.label)
    }
    // A pill's origin rounds from the sum of what leads to it, along the toolbar and across it.
    val attachEnd = (toolbarPadding + BUTTON_SIZE_DP + theme.spacing.xs).px - toolbarPadding.px - BUTTON_SIZE_DP.px
    val pillTop = ((BUTTON_SIZE_DP - PILL_HEIGHT_DP) / 2).px
    toolbar.addView(attach, LayoutParams(BUTTON_SIZE_DP.px, BUTTON_SIZE_DP.px).apply { marginEnd = attachEnd })
    toolbar.addView(modelButton, LayoutParams(LayoutParams.WRAP_CONTENT, PILL_HEIGHT_DP.px).apply {
      gravity = android.view.Gravity.TOP; topMargin = pillTop; marginEnd = theme.spacing.xs.px
    })
    toolbar.addView(thinkingButton, LayoutParams(LayoutParams.WRAP_CONTENT, PILL_HEIGHT_DP.px).apply {
      gravity = android.view.Gravity.TOP; topMargin = pillTop
    })
    toolbar.addView(accessoryHost, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f))
    toolbar.addView(dictate, LayoutParams(BUTTON_SIZE_DP.px, BUTTON_SIZE_DP.px))
    toolbar.addView(primaryAction, LayoutParams(BUTTON_SIZE_DP.px, BUTTON_SIZE_DP.px))
    field.addView(toolbar)
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
    attach.text = ""; attach.contentDescription = labels.addAttachment
    attach.foreground = ShadowListUtilsPlusIcon(theme.colors.label, 18f)
    attach.visibility = if (onPressAttach != null) VISIBLE else GONE
    modelButton.text = model.orEmpty(); modelButton.contentDescription = model?.let(labels.model)
    modelButton.setCompoundDrawablesRelativeWithIntrinsicBounds(null, null,
      if (onPressModel == null) null else ShadowListUtilsChevronIcon(theme.colors.secondaryLabel, 10f, 1.8f, ShadowListUtilsChevronIcon.Direction.DOWN), null)
    modelButton.compoundDrawablePadding = theme.spacing.xs.px
    modelButton.visibility = if (model != null) VISIBLE else GONE
    modelButton.isEnabled = onPressModel != null
    thinkingButton.text = labels.thinkingToggle
    thinkingButton.contentDescription = labels.thinkingToggleDescription
    thinkingButton.isSelected = thinking
    thinkingButton.visibility = if (onThinkingChange != null) VISIBLE else GONE
    dictate.text = if (dictating) "■" else "●"
    dictate.contentDescription = if (dictating) labels.stopDictating else labels.dictate
    dictate.visibility = if (onPressDictate != null) VISIBLE else GONE
    primaryAction.text = if (stop) "■" else ""
    primaryAction.contentDescription = if (stop) labels.stop else labels.send
    primaryAction.isEnabled = if (stop) onStop != null else draft && (!streaming || sendWhileStreaming) && onSend != null
    modelButton.background = android.graphics.drawable.GradientDrawable().apply { setColor(theme.colors.fill); cornerRadius = 16f.pxf }
    thinkingButton.background = android.graphics.drawable.GradientDrawable().apply { setColor(if (thinking) theme.colors.accentSoft else theme.colors.fill); cornerRadius = 16f.pxf }
    primaryAction.textSize = 22f
    primaryAction.setTextColor(theme.colors.background)
    primaryAction.foreground = if (stop) null else ShadowListUtilsArrowUpIcon(if (primaryAction.isEnabled) theme.colors.label else theme.colors.secondaryLabel, 18f, 2.4f)
    primaryAction.background = android.graphics.drawable.GradientDrawable().apply {
      shape = android.graphics.drawable.GradientDrawable.OVAL
      setColor(if (stop) theme.colors.label else if (primaryAction.isEnabled) theme.colors.accent else theme.colors.fill)
    }
    theme.typography.footnote.with(ShadowListUtilsFontWeight.SEMIBOLD).applyTo(modelButton, theme.colors.label)
    theme.typography.footnote.with(ShadowListUtilsFontWeight.SEMIBOLD).applyTo(thinkingButton, if (thinking) theme.colors.accent else theme.colors.label)
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

  private companion object {
    const val BUTTON_SIZE_DP = 32f
    const val PILL_HEIGHT_DP = 30f
  }
}
