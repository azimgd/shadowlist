package com.shadowlist.kit.example

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Parcelable
import android.util.SparseArray
import android.view.Gravity
import android.view.Menu
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import com.shadowlist.kit.SLKAnchorState
import com.shadowlist.kit.SLKDefaultItemAnimator
import com.shadowlist.kit.SLKListCell
import com.shadowlist.kit.SLKListChanges
import com.shadowlist.kit.SLKListView
import com.shadowlist.kit.SLKSwipeAction
import com.shadowlist.kit.SLKSwipeActionsConfiguration
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
class SectionsScreen(context: Context) : Screen, SLKListView.DataSource, SLKListView.Sections, SLKListView.Delegate {
  val list = SLKListView(context)
  var sections: List<Pair<String, List<Contact>>> = emptyList()
    private set
  private var fresh = 5000
  override val view: View get() = list

  override fun load() {
    list.setBackgroundColor(Theme.background)
    list.estimatedItemSize = 64.dpf
    list.stickySectionHeaders = true
    list.showsSeparators = true
    list.separatorInsetStart = 68.dp
    list.registerCell("contact") { ContactCell(it) }
    list.dataSource = this
    list.delegate = this
    group(List(400) { Contact(it) })
    list.reloadData()
    if (LaunchArgs.get("SLScenario") == "sections") TouchScript(list).after(1500) { FeatureScenario.sections(this) }
  }

  fun group(contacts: List<Contact>) {
    sections = contacts.sortedBy { it.author.name }.groupBy { it.author.name.take(1) }.toList().sortedBy { it.first }
  }

  val allContacts: List<Contact> get() = sections.flatMap { it.second }

  /*
   * A new traveller lands in its section. The list reads the sections again and keeps its place.
   */
  fun addTraveller() {
    group(allContacts + Contact(fresh++))
    list.insertItems(intArrayOf(0))
  }

  override fun numberOfItems(listView: SLKListView) = allContacts.size
  override fun numberOfSections(listView: SLKListView) = sections.size
  override fun numberOfItemsInSection(listView: SLKListView, section: Int) = sections[section].second.size
  override fun keyForSection(listView: SLKListView, section: Int) = sections[section].first
  override fun titleForHeaderInSection(listView: SLKListView, section: Int) = sections[section].first
  override fun titleForFooterInSection(listView: SLKListView, section: Int) = "${sections[section].second.size} travellers"
  override fun sectionIndexTitles(listView: SLKListView) = sections.map { it.first }

  override fun keyForItem(listView: SLKListView, index: Int) = allContacts[index].id

  override fun cellForItem(listView: SLKListView, index: Int): SLKListCell {
    val cell = listView.dequeueReusableCell<ContactCell>("contact")
    val contact = allContacts[index]
    // Every fifth traveller has a longer note, which wraps and makes the row taller.
    cell.show(contact, if (index % 5 == 0) "${contact.subtitle} · Window seat, travelling with two bags and a guitar case that goes in the cabin." else contact.subtitle)
    return cell
  }

  override fun didSelectItem(listView: SLKListView, index: Int) {
    listView.deselectItem(index, true)
  }
}

/*
 * A row of plain views measured by the list: its height follows the wrapped note.
 */
class ContactCell(context: Context) : SLKListCell(context, "contact") {
  private val avatar = TextView(context)
  private val name = TextView(context)
  private val note = TextView(context)

  init {
    setBackgroundColor(Theme.background)
    avatar.gravity = Gravity.CENTER
    avatar.setTextColor(Color.WHITE)
    avatar.textSize = 15f
    avatar.setTypeface(avatar.typeface, Typeface.BOLD)
    name.textSize = 17f
    name.setTextColor(Theme.label)
    note.textSize = 15f
    note.setTextColor(Theme.secondaryLabel)
    val column = LinearLayout(context).apply {
      orientation = LinearLayout.VERTICAL
      addView(name)
      addView(note)
    }
    val row = LinearLayout(context).apply {
      orientation = LinearLayout.HORIZONTAL
      setPadding(16.dp, 11.dp, 36.dp, 11.dp)
      addView(avatar, LinearLayout.LayoutParams(40.dp, 40.dp).apply { marginEnd = 12.dp })
      addView(column, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
    }
    addView(row)
  }

  fun show(contact: Contact, text: String) {
    avatar.text = contact.author.initials
    avatar.background = GradientDrawable().apply {
      shape = GradientDrawable.OVAL
      setColor(contact.author.color)
    }
    name.text = contact.author.name
    note.text = text
  }

  override fun setHighlighted(highlighted: Boolean, animated: Boolean) {
    super.setHighlighted(highlighted, animated)
    setBackgroundColor(if (highlighted) Theme.elevated2 else Theme.background)
  }
}

// endregion

// region Inbox

/*
 * One message of the inbox.
 */
class Message(index: Int) {
  val id = "message-$index"
  val author = Author(FixtureStrings.characterNames[index % FixtureStrings.characterNames.size])
  val text = FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.size]
  var read = index % 3 == 0
  var flagged = false
  var version = 0L
}

/*
 * An inbox that uses the list's interaction features: swipe actions, a context menu, selection
 * with editing, pull to refresh, separators, batch updates, applyChanges with content versions,
 * payload reloads, prefetching and a saved scroll position. Changes animate. The same screen as
 * the UIKit Inbox route.
 */
class InboxScreen(context: Context) : Screen, SLKListView.DataSource, SLKListView.Sizing, SLKListView.ContentVersions,
  SLKListView.Delegate, SLKListView.PrefetchDataSource {
  val list = SLKListView(context)
  val messages = ArrayList<Message>(List(200) { Message(it) })
  private var fresh = 1000
  var prefetched = 0
  var cancelled = 0
  var reconfigured = 0
  var refreshes = 0
  var menus = 0
  override val view: View get() = list

  override fun load() {
    list.setBackgroundColor(Theme.background)
    list.estimatedItemSize = 76.dpf
    list.animatesChanges = true
    list.showsSeparators = true
    list.separatorInsetStart = 72.dp
    list.refreshEnabled = true
    list.registerCell("message") { MessageCell(it) }
    list.dataSource = this
    list.delegate = this
    list.prefetchDataSource = this
    list.reloadData()
    if (LaunchArgs.get("SLScenario") == "features") TouchScript(list).after(1500) { FeatureScenario.inbox(this) }
  }

  fun toggleEditing() {
    list.setEditing(!list.editing, true)
    list.allowsMultipleSelection = list.editing
  }

  /*
   * New mail on top, a removed message and a moved one, all in one animated pass.
   * Before: m0 m1 m2 m3 m4 m5. After: new m5 m1 m2 m3 m4, with m2 marked read or unread.
   */
  fun batchUpdate() {
    if (messages.size <= 6) return
    val moved = messages.removeAt(5)
    messages.removeAt(0)
    messages.add(0, moved)
    messages.add(0, Message(fresh++))
    messages[3].read = !messages[3].read
    list.performBatchUpdates({
      list.deleteItems(intArrayOf(0))
      list.moveItem(5, 1)
      list.insertItems(intArrayOf(0))
      list.reloadItems(intArrayOf(2))
    })
  }

  /*
   * Shuffle a few messages, edit one and hand the list the new data to diff.
   */
  fun shuffle(): SLKListChanges {
    val reversed = messages.subList(2, minOf(12, messages.size)).reversed()
    for ((offset, message) in reversed.withIndex()) messages[2 + offset] = message
    messages[1].version += 1
    messages[1].flagged = !messages[1].flagged
    messages.add(4, Message(fresh++))
    messages.removeAt(messages.size - 1)
    return list.applyChanges()
  }

  fun addNewMail(count: Int) {
    messages.addAll(0, List(count) { Message(fresh + it) })
    fresh += count
    list.insertItems(IntArray(count) { it })
  }

  fun toggleRead(index: Int) {
    messages[index].read = !messages[index].read
    list.reloadItems(intArrayOf(index), "read")
  }

  fun delete(index: Int) {
    messages.removeAt(index)
    list.deleteItems(intArrayOf(index))
  }

  override fun numberOfItems(listView: SLKListView) = messages.size
  override fun keyForItem(listView: SLKListView, index: Int) = messages[index].id
  override fun sizeForItem(listView: SLKListView, index: Int, crossSize: Int) = 76.dp
  override fun contentVersionForItem(listView: SLKListView, index: Int) = messages[index].version

  override fun cellForItem(listView: SLKListView, index: Int): SLKListCell =
    listView.dequeueReusableCell<MessageCell>("message").also { it.show(messages[index]) }

  override fun reconfigureCell(listView: SLKListView, cell: SLKListCell, index: Int, payload: Any?): Boolean {
    if (cell !is MessageCell || payload != "read") return false
    reconfigured++
    cell.show(messages[index])
    return true
  }

  override fun prefetchItems(listView: SLKListView, indices: IntArray) {
    prefetched += indices.size
  }

  override fun cancelPrefetchingForItems(listView: SLKListView, indices: IntArray) {
    cancelled += indices.size
  }

  override fun didSelectItem(listView: SLKListView, index: Int) {
    if (!listView.editing) {
      listView.deselectItem(index, true)
      toggleRead(index)
    }
  }

  override fun leadingSwipeActionsForItem(listView: SLKListView, index: Int): SLKSwipeActionsConfiguration {
    val key = messages[index].id
    val read = SLKSwipeAction(SLKSwipeAction.Style.NORMAL, if (messages[index].read) "Unread" else "Read") { _, done ->
      val at = messages.indexOfFirst { it.id == key }
      if (at >= 0) toggleRead(at)
      done(at >= 0)
    }
    read.backgroundColor = Theme.accent
    return SLKSwipeActionsConfiguration(listOf(read))
  }

  override fun trailingSwipeActionsForItem(listView: SLKListView, index: Int): SLKSwipeActionsConfiguration {
    val key = messages[index].id
    val delete = SLKSwipeAction(SLKSwipeAction.Style.DESTRUCTIVE, "Delete") { _, done ->
      val at = messages.indexOfFirst { it.id == key }
      if (at >= 0) delete(at)
      done(at >= 0)
    }
    val flag = SLKSwipeAction(SLKSwipeAction.Style.NORMAL, "Flag") { _, done ->
      val at = messages.indexOfFirst { it.id == key }
      if (at >= 0) {
        messages[at].flagged = !messages[at].flagged
        list.reloadItems(intArrayOf(at), "read")
      }
      done(at >= 0)
    }
    flag.backgroundColor = 0xFFFF9500.toInt()
    return SLKSwipeActionsConfiguration(listOf(delete, flag))
  }

  override fun contextMenuForItem(listView: SLKListView, index: Int, menu: Menu): Boolean {
    menus++
    val key = messages[index].id
    fun at() = messages.indexOfFirst { it.id == key }
    menu.add("Pin to Top").setOnMenuItemClickListener {
      val from = at()
      if (from >= 0) {
        messages.add(0, messages.removeAt(from))
        list.moveItem(from, 0)
      }
      true
    }
    menu.add(if (messages[index].read) "Mark Unread" else "Mark Read").setOnMenuItemClickListener {
      at().takeIf { it >= 0 }?.let(::toggleRead)
      true
    }
    menu.add("Delete").setOnMenuItemClickListener {
      at().takeIf { it >= 0 }?.let(::delete)
      true
    }
    return true
  }

  override fun didBeginRefreshing(listView: SLKListView) {
    refreshes++
    listView.postDelayed({
      addNewMail(3)
      list.refreshing = false
    }, 1000)
  }
}

/*
 * A message row: avatar, sender, text, an unread dot and a flag. It shows highlight, selection
 * and, while editing, a check circle.
 */
class MessageCell(context: Context) : SLKListCell(context, "message") {
  private val avatar = TextView(context)
  private val sender = TextView(context)
  private val body = TextView(context)
  private val dot = View(context)
  private val flag = TextView(context)
  private val check = TextView(context)

  init {
    setBackgroundColor(Theme.background)
    avatar.gravity = Gravity.CENTER
    avatar.setTextColor(Color.WHITE)
    avatar.textSize = 15f
    sender.textSize = 16f
    sender.setTypeface(sender.typeface, Typeface.BOLD)
    sender.setTextColor(Theme.label)
    sender.maxLines = 1
    body.textSize = 14f
    body.setTextColor(Theme.secondaryLabel)
    body.maxLines = 2
    dot.background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(Theme.accent) }
    flag.text = "⚑"
    flag.setTextColor(0xFFFF9500.toInt())
    check.textSize = 20f
    check.setTextColor(Theme.accent)
    for (view in listOf(avatar, sender, body, dot, flag, check)) addView(view)
  }

  fun show(message: Message) {
    avatar.text = message.author.initials
    avatar.background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(message.author.color) }
    sender.text = message.author.name
    body.text = message.text
    dot.visibility = if (message.read) INVISIBLE else VISIBLE
    flag.visibility = if (message.flagged) VISIBLE else INVISIBLE
    contentDescription = "${message.author.name}, ${if (message.read) "read" else "unread"}, ${message.text}"
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    val lead = if (editing) 40.dp else 0
    fun exact(view: View, w: Int, h: Int) =
      view.measure(MeasureSpec.makeMeasureSpec(max(0, w), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(h, MeasureSpec.EXACTLY))
    exact(check, 24.dp, 24.dp)
    exact(dot, 10.dp, 10.dp)
    exact(avatar, 44.dp, 44.dp)
    exact(sender, width - lead - 72.dp - 40.dp, 22.dp)
    exact(body, width - lead - 72.dp - 16.dp, 40.dp)
    exact(flag, 20.dp, 22.dp)
    setMeasuredDimension(width, 76.dp)
  }

  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
    val lead = if (editing) 40.dp else 0
    check.layout(12.dp, 26.dp, 36.dp, 50.dp)
    check.alpha = if (editing) 1f else 0f
    dot.layout(lead + 4.dp, 33.dp, lead + 14.dp, 43.dp)
    avatar.layout(lead + 18.dp, 16.dp, lead + 62.dp, 60.dp)
    sender.layout(lead + 72.dp, 9.dp, lead + 72.dp + sender.measuredWidth, 31.dp)
    body.layout(lead + 72.dp, 31.dp, lead + 72.dp + body.measuredWidth, 71.dp)
    flag.layout(width - 34.dp, 9.dp, width - 14.dp, 31.dp)
  }

  override fun setHighlighted(highlighted: Boolean, animated: Boolean) {
    super.setHighlighted(highlighted, animated)
    updateBackground()
  }

  override fun setSelected(selected: Boolean, animated: Boolean) {
    super.setSelected(selected, animated)
    check.text = if (selected) "●" else "○"
    updateBackground()
  }

  override fun setEditing(editing: Boolean, animated: Boolean) {
    super.setEditing(editing, animated)
    check.text = if (isSelected) "●" else "○"
    requestLayout()
  }

  private fun updateBackground() {
    setBackgroundColor(if (highlighted) Theme.elevated2 else if (isSelected) Theme.elevated else Theme.background)
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
    val top = list.scrollY + list.paddingTop
    result.put("headerOffsetAfterScrollToSection", (list.rectForHeaderInSection(5)?.top ?: -1f) - top)
    // Scroll into the section: its header stays pinned at the top.
    list.scrollBy(0, 120.dp)
    relayout(list)
    val viewportTop = list.scrollY + list.paddingTop
    val pinned = (0 until list.childCount).map { list.getChildAt(it) }.filterIsInstance<SLKListCell>()
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

    // Selection follows keys.
    list.allowsMultipleSelection = true
    list.selectItem(1)
    list.selectItem(3)
    list.deselectItem(1)
    screen.messages.removeAt(0)
    list.deleteItems(intArrayOf(0))
    relayout(list)
    result.put("selectedAfterDelete", JSONArray(list.selectedIndices.toList()))
    list.allowsMultipleSelection = false

    // A batch: new mail, a delete, a move and a reload.
    screen.batchUpdate()
    relayout(list)
    result.put("batchConsistent", consistent(screen))

    // applyChanges diffs the new data.
    val changes = screen.shuffle()
    relayout(list)
    result.put("applyChanges", JSONObject().put("deleted", changes.deletedIndices.size).put("inserted", changes.insertedIndices.size)
      .put("moved", changes.movedFromIndices.size).put("reloaded", changes.reloadedIndices.size))
    result.put("applyConsistent", consistent(screen))

    // A payload reload updates the shown cell in place.
    val cellBefore = list.cellForItem(2)
    screen.toggleRead(2)
    relayout(list)
    result.put("payloadKeptCell", cellBefore != null && cellBefore === list.cellForItem(2))
    result.put("reconfigured", screen.reconfigured)

    // Swipe the third row all the way across with synthesized touches: its delete runs.
    val count = screen.messages.size
    val swiped = screen.messages[2].id
    val row = list.rectForItem(2) ?: return report(result.put("error", "no row 2"))
    val y = row.centerY() - list.scrollY
    val startX = list.width - 40f
    script.down(startX, y)
    script.frames(20, { frame -> script.move(startX - (frame + 1) * list.width / 18f, y) }) {
      script.up(startX - list.width * 1.1f, y)
      script.after(600) {
        relayout(list)
        result.put("fullSwipeDeleted", screen.messages.size == count - 1 && screen.messages.none { it.id == swiped })
        partialSwipe(screen, result)
      }
    }
  }

  /*
   * A partial swipe opens a row. Closing it puts it back. Then the menu, refresh, saved place.
   */
  private fun partialSwipe(screen: InboxScreen, result: JSONObject) {
    val list = screen.list
    val script = TouchScript(list)
    val row = list.rectForItem(1) ?: return report(result)
    val y = row.centerY() - list.scrollY
    script.down(40f, y)
    script.frames(8, { frame -> script.move(40f + (frame + 1) * 15.dpf, y) }) {
      script.up(40f + 120.dpf, y)
      script.after(400) {
        result.put("swipeOpenOffset", (list.cellForItem(1)?.translationX ?: 0f) / density)
        list.closeSwipeActions(false)
        result.put("swipeClosedOffset", list.cellForItem(1)?.translationX ?: -1f)
        longPress(screen, result)
      }
    }
  }

  private fun longPress(screen: InboxScreen, result: JSONObject) {
    val list = screen.list
    val script = TouchScript(list)
    val row = list.rectForItem(4) ?: return report(result)
    val y = row.centerY() - list.scrollY
    script.down(list.width / 2f, y)
    script.after(800) {
      script.up(list.width / 2f, y)
      result.put("contextMenu", screen.menus > 0)
      pullToRefresh(screen, result)
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
      result.put("refreshDelegateCalls", screen.refreshes)
      script.after(1500) {
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

  private fun anchorError(screen: InboxScreen, place: SLKAnchorState): Float {
    val list = screen.list
    val index = screen.messages.indexOfFirst { it.id == place.key }
    val viewportTop = list.scrollY + list.paddingTop
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
      if (cell.contentDescription?.startsWith(screen.messages[index].author.name) != true) return false
    }
    return list.rectForItem(screen.messages.size - 1) != null && list.rectForItem(screen.messages.size) == null
  }

  private fun visibleItems(list: SLKListView): Map<String, Int> =
    list.visibleCells.associate { (list.dataSource!!.keyForItem(list, it.index)) to (it.top - list.scrollY) }

  private fun relayout(list: SLKListView) {
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
class CountingAnimator : SLKDefaultItemAnimator() {
  var calls = 0
    private set

  override fun animateInsert(listView: SLKListView, cell: SLKListCell) {
    calls++
    super.animateInsert(listView, cell)
  }

  override fun animateRemoval(listView: SLKListView, cell: SLKListCell, completion: () -> Unit) {
    calls++
    super.animateRemoval(listView, cell, completion)
  }

  override fun animateMove(listView: SLKListView, cell: SLKListCell, fromX: Float, fromY: Float) {
    calls++
    super.animateMove(listView, cell, fromX, fromY)
  }
}

// endregion
