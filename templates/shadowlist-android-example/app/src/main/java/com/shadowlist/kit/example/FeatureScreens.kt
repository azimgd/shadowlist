package com.shadowlist.kit.example

import android.content.Context
import android.os.Parcelable
import android.util.Log
import android.util.SparseArray
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import com.shadowlist.kit.ShadowListKitAnchorState
import com.shadowlist.kit.ShadowListKitDefaultItemAnimator
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitListView
import com.shadowlist.utils.ShadowListUtilsAvatar
import com.shadowlist.utils.ShadowListUtilsContactItem
import com.shadowlist.utils.ShadowListUtilsContactSection
import com.shadowlist.utils.ShadowListUtilsInboxList
import com.shadowlist.utils.ShadowListUtilsInboxMessage
import com.shadowlist.utils.ShadowListUtilsSectionHeader
import com.shadowlist.utils.ShadowListUtilsSectionHeaderView
import com.shadowlist.utils.shadowListUtilsGroupIntoSections
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.max

// region Sections

/*
 * Travellers grouped by the first letter of their name: built-in section headers that stick,
 * a count in each section's footer, a section index, separators and cells that measure
 * themselves. The data source gives no sizes. The same screen as the UIKit Sections route.
 */
class SectionsScreen(context: Context) : Screen, ShadowListKitListView.DataSource, ShadowListKitListView.Sections, ShadowListKitListView.Delegate {
  val list = ShadowListKitListView(context)
  var sections: List<ShadowListUtilsContactSection> = emptyList()
    private set
  private var fresh = 5000
  override val view: View get() = list

  override fun load() {
    list.setBackgroundColor(view.context.listTheme.colors.background)
    list.separatorColor = view.context.listTheme.colors.separator
    list.estimatedItemSizePx = 64.dpf
    list.stickySectionHeaders = true
    list.renderStickyHeaderOverlay = { _, _, section ->
      if (section in sections.indices) ShadowListUtilsSectionHeaderView(view.context).also { show(section, it) } else null
    }
    list.showsSeparators = true
    list.separatorInsetStartPx = view.context.listTheme.rowInset.dp
    list.registerCell("contact") { ContactCell(it) }
    list.registerCell("header") { ShadowListUtilsSectionHeaderView(it) }
    list.registerCell("footer") { ListFooterCell(it) }
    list.dataSource = this
    list.delegate = this
    group(List(400) { Items.contact(Contact(it)) })
    list.reloadData()
    if (LaunchArgs.get("SLScenario") == "sections") TouchScript(list).after(1500) { FeatureScenario.sections(this) }
  }

  override fun headerMenu() = listOf(listOf(HeaderAction("Add Traveller", onPress = ::addTraveller)))

  /*
   * A section's header in a header view, which jumps to the next section on a tap.
   */
  private fun show(section: Int, header: ShadowListUtilsSectionHeaderView) {
    val data = sections[section]
    val title = "${data.title} · ${data.items.size} travellers · Next section"
    header.configure(ShadowListUtilsSectionHeader(data.key, title, theme = view.context.listTheme), null)
    header.setOnClickListener { showNextSection(data.key) }
  }

  private fun showNextSection(key: String) {
    val section = sections.indexOfFirst { it.key == key }
    if (section >= 0) list.scrollToSection((section + 1) % sections.size, animated = true)
  }

  fun group(contacts: List<ShadowListUtilsContactItem>) {
    sections = shadowListUtilsGroupIntoSections(contacts.sortedBy { it.name })
  }

  val allContacts: List<ShadowListUtilsContactItem> get() = sections.flatMap { it.items }

  /*
   * A new traveller lands in its section. The list reads the sections again and keeps its place.
   */
  fun addTraveller() {
    group(allContacts + Items.contact(Contact(fresh++)))
    list.insertItems(intArrayOf(0))
  }

  override fun numberOfItems(listView: ShadowListKitListView) = allContacts.size
  override fun numberOfSections(listView: ShadowListKitListView) = sections.size
  override fun numberOfItemsInSection(listView: ShadowListKitListView, section: Int) = sections[section].items.size
  override fun keyForSection(listView: ShadowListKitListView, section: Int) = sections[section].key
  override fun titleForHeaderInSection(listView: ShadowListKitListView, section: Int) = sections[section].title
  override fun titleForFooterInSection(listView: ShadowListKitListView, section: Int) = "${sections[section].items.size} travellers"

  override fun cellForHeaderInSection(listView: ShadowListKitListView, section: Int): ShadowListKitListCell =
    listView.dequeueReusableCell<ShadowListUtilsSectionHeaderView>("header").also { show(section, it) }

  override fun cellForFooterInSection(listView: ShadowListKitListView, section: Int): ShadowListKitListCell =
    listView.dequeueReusableCell<ListFooterCell>("footer").also { it.configure("${sections[section].items.size} travellers") }

  override fun sectionIndexTitles(listView: ShadowListKitListView) = sections.map { it.title }

  override fun keyForItem(listView: ShadowListKitListView, index: Int) = allContacts[index].key

  override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell {
    val cell = listView.dequeueReusableCell<ContactCell>("contact")
    val contact = allContacts[index]
    // Every fifth traveller has a longer note, which wraps and makes the row taller.
    val note = contact.subtitle.orEmpty()
    cell.show(contact, if (index % 5 == 0) "$note · Window seat, travelling with two bags and a guitar case that goes in the cabin." else note)
    return cell
  }

  override fun didSelectItem(listView: ShadowListKitListView, index: Int) {
    listView.deselectItem(index, true)
  }
}

/*
 * A row of plain views measured by the list: its height follows the wrapped note.
 */
class ContactCell(context: Context) : ShadowListKitListCell(context, "contact") {
  private val theme = context.listTheme
  private val avatar = ShadowListUtilsAvatar(context, theme = theme)
  private val name = TextView(context)
  private val note = TextView(context)

  init {
    setBackgroundColor(theme.colors.background)
    val column = LinearLayout(context).apply {
      orientation = LinearLayout.VERTICAL
      addView(name)
      addView(note)
    }
    val row = LinearLayout(context).apply {
      orientation = LinearLayout.HORIZONTAL
      setPadding(theme.spacing.lg.dp, PADDING_VERTICAL_DP.dp, PADDING_END_DP.dp, PADDING_VERTICAL_DP.dp)
      addView(avatar, LinearLayout.LayoutParams(avatar.size.dp, avatar.size.dp).apply { marginEnd = theme.spacing.md.dp })
      addView(column, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
    }
    addView(row)
  }

  fun show(contact: ShadowListUtilsContactItem, text: String) {
    avatar.show(contact.name)
    name.text = contact.name
    note.text = text
    theme.typography.body.applyTo(name, theme.colors.label)
    theme.typography.subhead.applyTo(note, theme.colors.secondaryLabel)
  }

  override fun setHighlighted(highlighted: Boolean, animated: Boolean) {
    super.setHighlighted(highlighted, animated)
    setBackgroundColor(if (highlighted) theme.colors.elevated2 else theme.colors.background)
  }

  private companion object {
    const val PADDING_VERTICAL_DP = 11
    const val PADDING_END_DP = 36
  }
}

// endregion

// region Inbox

/*
 * An inbox on the library's template: swipe actions, a menu on a hold, selection while editing,
 * pull to refresh and separators, with animated changes. The screen keeps the messages and hands
 * the template a new list for every change. The menu's Select starts editing, and deselecting
 * the last message ends it. The same screen as the UIKit Inbox route.
 */
class InboxScreen(context: Context) : Screen, ShadowListKitListView.PrefetchDataSource {
  val list = ShadowListUtilsInboxList(context)
  var messages: List<ShadowListUtilsInboxMessage> = List(200) { Items.mail(it) }
    private set
  private var fresh = 1000
  var prefetched = 0
  var cancelled = 0
  var refreshes = 0
  var pressed = 0
  var selections = 0
  override val view: View get() = list

  override fun load() {
    list.prefetchDataSource = this
    list.onPressItem = {
      pressed++
      toggleRead(it)
    }
    list.onToggleRead = ::toggleRead
    list.onToggleFlag = { message -> replace(message.copy(flagged = !message.flagged)) }
    list.onDelete = ::delete
    list.onReply = { Log.i("SLEXAMPLE", "reply to ${it.sender}") }
    list.onSelectionChange = { keys ->
      selections++
      // The menu's Select hands over one key while the list is not editing yet.
      if (!list.editing) {
        list.setEditing(true, true)
        list.selectedKeys = keys
      } else if (keys.isEmpty()) {
        list.setEditing(false, true)
      }
    }
    list.onRefresh = {
      refreshes++
      list.postDelayed({
        addNewMail(3)
        list.refreshing = false
      }, 1000)
    }
    list.items = messages
    if (Settings.selectDemo) {
      list.postDelayed({
        list.setEditing(true, false)
        list.selectedKeys = messages.subList(1, 3).mapTo(HashSet()) { it.key }
      }, SELECT_DEMO_DELAY_MS)
    }
    if (LaunchArgs.get("SLScenario") == "features") TouchScript(list).after(1500) { FeatureScenario.inbox(this) }
  }

  private fun show(next: List<ShadowListUtilsInboxMessage>) {
    messages = next
    list.setItems(next, true)
  }

  private fun replace(message: ShadowListUtilsInboxMessage) = show(messages.map { if (it.key == message.key) message else it })

  fun toggleRead(message: ShadowListUtilsInboxMessage) = replace(message.copy(read = !message.read))

  fun delete(message: ShadowListUtilsInboxMessage) = show(messages.filter { it.key != message.key })

  private fun newMail() =
    Items.mail(fresh++).copy(read = false, receivedAtMs = System.currentTimeMillis().toDouble())

  /*
   * New mail on top. The visible messages stay where they are.
   */
  fun addNewMail(count: Int) {
    show(List(count) { newMail() } + messages)
  }

  /*
   * Reverse a few messages, flag one, add one and drop the last, in one change the list diffs.
   */
  fun shuffle() {
    val next = ArrayList(messages)
    val reversed = next.subList(2, minOf(12, next.size)).reversed()
    for ((offset, message) in reversed.withIndex()) next[2 + offset] = message
    next[1] = next[1].copy(flagged = !next[1].flagged)
    next.add(4, newMail())
    next.removeAt(next.size - 1)
    show(next)
  }

  override fun prefetchItems(listView: ShadowListKitListView, indices: IntArray) {
    prefetched += indices.size
  }

  override fun cancelPrefetchingForItems(listView: ShadowListKitListView, indices: IntArray) {
    cancelled += indices.size
  }

  private companion object {
    const val SELECT_DEMO_DELAY_MS = 1000L
  }
}

// endregion

// region Scenarios

/*
 * Scripted checks of the feature screens: --es SLRoute Sections --es SLScenario sections and
 * --es SLRoute Inbox --es SLScenario features. Each logs one SLSCENARIO JSON line, the same
 * fields as the UIKit example.
 */
object FeatureScenario {
  private val exit: Boolean get() = LaunchArgs.get("SLBenchExit") == "1"

  fun sections(screen: SectionsScreen) {
    val list = screen.list
    val script = TouchScript(list)
    val result = JSONObject().put("scenario", "sections")
    result.put("numberOfSections", list.numberOfSections)
    result.put("sectionOfThirdsFirstItem", list.sectionForItem(list.firstItemIndexInSection(3)))
    // Self-sizing: a row with the long note is taller than one without.
    result.put("selfSizedShort", list.rectForItem(1)?.height()?.div(density) ?: -1f)
    result.put("selfSizedTall", list.rectForItem(0)?.height()?.div(density) ?: -1f)
    list.scrollToSection(5)
    relayout(list)
    result.put("headerOffsetAfterScrollToSection", (list.rectForHeaderInSection(5)?.top ?: -1f) - list.scrollY)
    // Scroll into the section: its header stays pinned at the top.
    list.scrollBy(0, 120.dp)
    relayout(list)
    val viewportTop = list.scrollY + list.paddingTop
    val pinned = (0 until list.childCount).map { list.getChildAt(it) }.filterIsInstance<ShadowListKitListCell>()
      .filter { it.visibility == View.VISIBLE && it.index < 0 && it.top <= viewportTop + 1 && it.bottom > viewportTop }
    result.put("pinnedHeaderOffset", pinned.firstOrNull()?.let { (it.top - viewportTop) / density } ?: JSONObject.NULL)
    val before = visibleItems(list)
    screen.addTraveller()
    relayout(list)
    val after = visibleItems(list)
    var shift = 0
    for ((key, y) in before) after[key]?.let { shift = max(shift, abs(it - y)) }
    result.put("maxShiftAfterRegroup", shift)
    script.after(300) { report(result) }
  }

  fun inbox(screen: InboxScreen) {
    val list = screen.list
    val script = TouchScript(list)
    val result = JSONObject().put("scenario", "features")
    val animator = CountingAnimator()
    list.itemAnimator = animator

    // Selection follows keys while editing, and goes when editing ends.
    list.setEditing(true, false)
    list.selectedKeys = setOf(screen.messages[1].key, screen.messages[3].key)
    screen.delete(screen.messages[0])
    relayout(list)
    result.put("selectedAfterDelete", JSONArray(list.selectedIndices.toList()))
    list.setEditing(false, false)
    relayout(list)
    result.put("selectedAfterEditing", list.selectedKeys.size)

    // Several messages change at once and the list diffs them.
    screen.shuffle()
    relayout(list)
    result.put("shuffleConsistent", consistent(screen))

    // A message marked read shows its new state under the same key.
    val toggled = screen.messages[2]
    screen.toggleRead(toggled)
    relayout(list)
    result.put("toggledReadShown", list.cellForItem(2)?.contentDescription?.startsWith("Unread") == toggled.read)

    // Swipe the third row all the way across with synthesized touches: its delete runs.
    val count = screen.messages.size
    val swiped = screen.messages[2].key
    val row = list.rectForItem(2) ?: return report(result.put("error", "no row 2"))
    val y = row.centerY() + list.paddingTop - list.scrollY
    val startX = list.width - 40f
    script.down(startX, y)
    script.frames(20, { frame -> script.move(startX - (frame + 1) * list.width / 18f, y) }) {
      script.up(startX - list.width * 1.1f, y)
      script.after(600) {
        relayout(list)
        result.put("fullSwipeDeleted", screen.messages.size == count - 1 && screen.messages.none { it.key == swiped })
        partialSwipe(screen, result)
      }
    }
  }

  /*
   * A partial swipe opens a row. Closing it puts it back. Then a tap, refresh and the saved place.
   */
  private fun partialSwipe(screen: InboxScreen, result: JSONObject) {
    val list = screen.list
    val script = TouchScript(list)
    val row = list.rectForItem(1) ?: return report(result)
    val y = row.centerY() + list.paddingTop - list.scrollY
    script.down(40f, y)
    script.frames(8, { frame -> script.move(40f + (frame + 1) * 15.dpf, y) }) {
      script.up(40f + 120.dpf, y)
      script.after(400) {
        result.put("swipeOpenOffset", (list.cellForItem(1)?.translationX ?: 0f) / density)
        list.closeSwipeActions(false)
        result.put("swipeClosedOffset", list.cellForItem(1)?.translationX ?: -1f)
        tap(screen, result)
      }
    }
  }

  /*
   * A tap presses the message, which this screen marks read or unread.
   */
  private fun tap(screen: InboxScreen, result: JSONObject) {
    val list = screen.list
    val script = TouchScript(list)
    val row = list.rectForItem(4) ?: return report(result)
    val y = row.centerY() + list.paddingTop - list.scrollY
    val before = screen.messages[4]
    script.down(list.width / 2f, y)
    script.after(60) {
      script.up(list.width / 2f, y)
      script.after(400) {
        result.put("tapPressed", screen.pressed).put("tapToggledRead", screen.messages[4].read != before.read)
        pullToRefresh(screen, result)
      }
    }
  }

  private fun pullToRefresh(screen: InboxScreen, result: JSONObject) {
    val list = screen.list
    val script = TouchScript(list)
    list.scrollToStart()
    relayout(list)
    script.down(list.width / 2f, 100.dpf)
    script.frames(20, { frame -> script.move(list.width / 2f, 100.dpf + (frame + 1) * 12.dpf) }) {
      script.up(list.width / 2f, 340.dpf)
      result.put("refreshCalls", screen.refreshes)
      var before: Map<String, Int> = emptyMap()
      script.after(800) { before = visibleItems(list) }
      script.after(1800) {
        val after = visibleItems(list)
        result.put("refreshShift", before.entries.maxOfOrNull { (key, y) -> after[key]?.let { abs(it - y) } ?: 0 } ?: -1)
        result.put("refreshingAfterDone", list.refreshing)
        savedPlace(screen, result)
      }
    }
  }

  private fun savedPlace(screen: InboxScreen, result: JSONObject) {
    val list = screen.list
    list.scrollToItem(60)
    relayout(list)
    list.scrollBy(0, 13.dp)
    relayout(list)
    val place = list.anchorState ?: return report(result.put("error", "no anchor"))
    screen.addNewMail(5)
    list.scrollToStart()
    relayout(list)
    list.restoreAnchorState(place)
    relayout(list)
    result.put("anchorKey", place.key)
    result.put("anchorError", anchorError(screen, place))
    // The same through the view's saved state, like a configuration change.
    list.id = View.generateViewId()
    val container = SparseArray<Parcelable>()
    list.saveHierarchyState(container)
    list.scrollToStart()
    relayout(list)
    list.restoreHierarchyState(container)
    relayout(list)
    result.put("savedStateError", anchorError(screen, place))
    result.put("prefetched", screen.prefetched)
    result.put("cancelled", screen.cancelled)
    result.put("animatorCalls", (list.itemAnimator as CountingAnimator).calls)
    report(result)
  }

  private fun anchorError(screen: InboxScreen, place: ShadowListKitAnchorState): Float {
    val list = screen.list
    val index = screen.messages.indexOfFirst { it.key == place.key }
    val viewportTop = list.scrollY
    val top = list.rectForItem(index)?.top ?: return -1f
    return (viewportTop - top) - place.offset
  }

  /*
   * Whether every shown cell shows the message at its index, and the list holds them all.
   */
  private fun consistent(screen: InboxScreen): Boolean {
    val list = screen.list
    val range = list.visibleRange ?: return false
    for (index in range) {
      val cell = list.cellForItem(index) ?: continue
      if (cell.contentDescription?.contains(screen.messages[index].preview) != true) return false
    }
    return list.rectForItem(screen.messages.size - 1) != null && list.rectForItem(screen.messages.size) == null
  }

  private fun visibleItems(list: ShadowListKitListView): Map<String, Int> {
    val source = list.dataSource ?: return emptyMap()
    return list.visibleCells.associate { source.keyForItem(list, it.index) to (it.top - list.scrollY) }
  }

  private fun relayout(list: ShadowListKitListView) {
    list.measure(View.MeasureSpec.makeMeasureSpec(list.width, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(list.height, View.MeasureSpec.EXACTLY))
    list.layout(list.left, list.top, list.right, list.bottom)
  }

  private val density: Float get() = 1.dpf

  private fun report(result: JSONObject) {
    android.util.Log.i("SLSCENARIO", result.toString())
    if (exit) android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({ android.os.Process.killProcess(android.os.Process.myPid()) }, 300)
  }
}

/*
 * The default animations, counted.
 */
class CountingAnimator : ShadowListKitDefaultItemAnimator() {
  var calls = 0
    private set

  override fun animateInsert(listView: ShadowListKitListView, cell: ShadowListKitListCell) {
    calls++
    super.animateInsert(listView, cell)
  }

  override fun animateDelete(listView: ShadowListKitListView, cell: ShadowListKitListCell, completion: () -> Unit) {
    calls++
    super.animateDelete(listView, cell, completion)
  }

  override fun animateMove(listView: ShadowListKitListView, cell: ShadowListKitListCell, fromX: Float, fromY: Float) {
    calls++
    super.animateMove(listView, cell, fromX, fromY)
  }
}

// endregion
