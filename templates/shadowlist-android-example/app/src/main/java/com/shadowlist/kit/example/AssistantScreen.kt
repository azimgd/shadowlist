package com.shadowlist.kit.example

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.view.View
import android.view.WindowInsets
import android.widget.Button
import android.widget.LinearLayout
import com.shadowlist.utils.*

/*
 * A local conversation fixture. Continue advances a real live turn; Finish, Stop and Fail send
 * typed terminal states back to the transcript. No view owns the application's messages.
 */
class AssistantScreen(private val context: Context) : Screen {
  private companion object {
    const val KEYBOARD_GAP_DP = 8
  }

  private val theme = ShadowListUtilsTheme.system(context)
  val list = ShadowListUtilsAssistantList(context, theme)
  val composer = ShadowListUtilsAssistantComposer(context, theme)
  private val root = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
  private val controls = LinearLayout(context)
  private val next = Button(context).apply { text = "Continue" }
  private val finish = Button(context).apply { text = "Finish" }
  private val fail = Button(context).apply { text = "Fail" }
  private val latest = Button(context).apply { text = "Scroll to latest"; visibility = View.GONE }
  private var serial = 1
  private var activeKey: String? = null
  private var chunk = 0
  private var editingKey: String? = null
  private val empty = ShadowListUtilsAssistantEmpty(context, theme)
  private val transcript = android.widget.FrameLayout(context)
  private var items: List<ShadowListUtilsAssistantMessage> = emptyList()
  private var turns = emptyMap<String, ShadowListUtilsAssistantTurn>()
  override val view: View get() = root

  override fun load() {
    for (button in listOf(next, finish, fail)) {
      button.isAllCaps = false
      button.textSize = 13f
      button.setTextColor(theme.colors.accent)
      button.background = android.graphics.drawable.GradientDrawable().apply { setColor(theme.colors.accentSoft); cornerRadius = 16.dpf }
      button.stateListAnimator = null
      controls.addView(button, LinearLayout.LayoutParams(0, 36.dp, 1f).apply { setMargins(4.dp, 4.dp, 4.dp, 4.dp) })
    }
    root.addView(controls)
    transcript.addView(list, android.widget.FrameLayout.LayoutParams(-1, -1))
    empty.labels = ShadowListUtilsAssistantLabels(emptyTitle = "Where are we flying?", emptySubtitle = "Plan flights, weather and stays.")
    empty.suggestions = listOf(
      ShadowListUtilsAssistantSuggestion("Write an itinerary", "Write a three-day itinerary for Lisbon in May"),
      ShadowListUtilsAssistantSuggestion("Compare fares", "Compare fares to Tokyo across airlines in a table"),
      ShadowListUtilsAssistantSuggestion("Search flights", "Search Skyfy for flights to Reykjavik"),
      ShadowListUtilsAssistantSuggestion("Try a dropped connection", "Simulate a dropped connection error"))
    empty.onSelectSuggestion = { send(it.prompt, emptyList()) }
    transcript.addView(empty, android.widget.FrameLayout.LayoutParams(-1, -2))
    root.addView(transcript, LinearLayout.LayoutParams(-1, 0, 1f))
    root.addView(latest)
    root.addView(composer, LinearLayout.LayoutParams(-1, -2))
    // The column settles a gap above the keyboard while it is up, as the React Native screen does.
    root.setOnApplyWindowInsetsListener { _, insets ->
      val keyboard = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && insets.isVisible(WindowInsets.Type.ime())
      root.setPadding(0, 0, 0, if (keyboard) KEYBOARD_GAP_DP.dp else 0)
      insets
    }
    next.setOnClickListener { advance() }
    finish.setOnClickListener { complete(ShadowListUtilsAssistantTurnStatus.Done(
      listOf(ShadowListUtilsAssistantSource("source-$serial", "Native list", "https://example.com/native-list")), listOf("Explain the next step", "Show another example"))) }
    fail.setOnClickListener { complete(ShadowListUtilsAssistantTurnStatus.Failed("The example request failed. Retry to start another turn.")) }
    composer.onSend = ::send
    composer.onStop = { complete(ShadowListUtilsAssistantTurnStatus.Stopped) }
    composer.thinking = true
    composer.model = "Cruise"
    composer.onPressModel = { composer.model = if (composer.model == "Cruise") "Reasoning" else "Cruise" }
    composer.onThinkingChange = { composer.thinking = it }
    composer.onPressAttach = {
      composer.attachments = composer.attachments + ShadowListUtilsAssistantAttachment("file-${serial++}", ShadowListUtilsAssistantAttachment.Kind.FILE,
        "itinerary.md", "Example attachment")
    }
    composer.onRemoveAttachment = { key -> composer.attachments = composer.attachments.filterNot { it.key == key } }
    composer.onCancelEdit = { editingKey = null; composer.editing = false; composer.clearDraft() }
    list.onCopy = ::copy
    list.onCopyCode = ::copy
    list.onShare = { text -> context.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text), "Share response").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) }
    list.onEdit = { key ->
      val prompt = (items.firstOrNull { it.key == key } as? ShadowListUtilsAssistantMessage.User)?.prompt
      if (prompt != null) { editingKey = key; composer.editing = true; composer.setDraft(prompt.text) }
    }
    list.onSelectVariant = { key, index -> updateReply(key) { it.copy(variantIndex = index) } }
    list.onFeedback = { key, feedback -> updateReply(key) { it.copy(feedback = feedback) } }
    list.onRegenerate = ::regenerate
    list.onRetry = ::regenerate
    list.onFollowUp = { send(it, emptyList()) }
    list.onScroll = { latest.visibility = if (list.atEnd) View.GONE else View.VISIBLE }
    latest.setOnClickListener { list.scrollToEnd(true); latest.visibility = View.GONE }
    show()
  }

  private fun send(text: String, attachments: List<ShadowListUtilsAssistantAttachment>) {
    if (activeKey != null) return
    val editing = editingKey
    val base = items.filterNot { it is ShadowListUtilsAssistantMessage.End }.toMutableList()
    val key = if (editing == null) "user-${serial++}" else editing
    if (editing != null) {
      val index = base.indexOfFirst { it.key == editing }
      if (index >= 0) { while (base.size > index) base.removeAt(base.lastIndex) }
    }
    val replyKey = "reply-${serial++}"
    base.add(ShadowListUtilsAssistantMessage.User(ShadowListUtilsAssistantPrompt(key, text, attachments)))
    base.add(ShadowListUtilsAssistantMessage.Assistant(ShadowListUtilsAssistantReply(replyKey, listOf(ShadowListUtilsAssistantTurn()), model = composer.model)))
    base.add(ShadowListUtilsAssistantMessage.endMarker)
    items = base
    composer.attachments = emptyList()
    composer.editing = false
    editingKey = null
    begin(replyKey)
  }

  private fun begin(key: String) {
    activeKey = key
    chunk = 0
    turns = turns + (key to ShadowListUtilsAssistantTurn(thinking = "Checking the current viewport and its stable keys."))
    show()
  }

  private fun advance() {
    val key = activeKey ?: return
    val current = turns[key] ?: return
    val nextContent = current.content + when (chunk++) {
      0 -> "## Native anchors\nThe visible **row key** stays in place.\n\n"
      1 -> "A later measurement grows the row and the shared core applies its correction.\n\n"
      else -> "```kotlin\nlist.scrollToEnd(true)\n```\n"
    }
    val calls = listOf(ShadowListUtilsAssistantToolCall("tool-$key", "Inspect rows", "visibleRange", 18,
      if (chunk < 2) ShadowListUtilsAssistantToolStatus.Running else ShadowListUtilsAssistantToolStatus.Done("The first visible row did not move.")))
    turns = turns + (key to current.copy(content = nextContent, thinkingMs = 1600.0, toolCalls = calls))
    show()
  }

  private fun complete(status: ShadowListUtilsAssistantTurnStatus) {
    val key = activeKey ?: return
    val current = turns[key] ?: return
    val finished = current.copy(status = status, thinkingMs = current.thinkingMs ?: 1600.0)
    updateReply(key, show = false) { reply ->
      val variants = reply.variants.toMutableList()
      if (reply.variantIndex in variants.indices) variants[reply.variantIndex] = finished else variants.add(finished)
      reply.copy(variants = variants)
    }
    turns = turns - key
    activeKey = null
    show()
  }

  private fun regenerate(key: String) {
    if (activeKey != null) return
    updateReply(key, show = false) { it.copy(variants = it.variants + ShadowListUtilsAssistantTurn(), variantIndex = it.variants.size) }
    begin(key)
  }

  private fun updateReply(key: String, show: Boolean = true, update: (ShadowListUtilsAssistantReply) -> ShadowListUtilsAssistantReply) {
    items = items.map { if (it is ShadowListUtilsAssistantMessage.Assistant && it.key == key) ShadowListUtilsAssistantMessage.Assistant(update(it.reply)) else it }
    if (show) show()
  }

  private fun show() {
    val streaming = activeKey != null
    empty.visibility = if (items.isEmpty()) View.VISIBLE else View.GONE
    controls.visibility = if (streaming) View.VISIBLE else View.GONE
    list.nonAnchorKeys = setOf(ShadowListUtilsAssistantMessage.endMarker.key) + listOfNotNull(activeKey)
    list.streaming = streaming
    composer.streaming = streaming
    list.setItems(items, false)
    list.setTurns(turns, false)
    next.isEnabled = streaming
    finish.isEnabled = streaming
    fail.isEnabled = streaming
  }

  private fun copy(text: String) {
    (context.getSystemService(Context.CLIPBOARD_SERVICE) as? ClipboardManager)?.setPrimaryClip(ClipData.newPlainText("Assistant", text))
  }
}
