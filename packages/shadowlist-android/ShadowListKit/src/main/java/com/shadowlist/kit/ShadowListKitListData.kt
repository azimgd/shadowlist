package com.shadowlist.kit

import android.util.Log
import com.shadowlist.kit.ShadowListKitListView.ContentVersions
import com.shadowlist.kit.ShadowListKitListView.Sections
import kotlin.math.max
import kotlin.math.min

/*
 * The list's data: row keys, sections and the changes that move them. The keys are the same
 * list the core holds. Reloads, inserts, deletes, batches and applyChanges update both and
 * report the change to the list, which mounts from the keys here.
 */
internal class ShadowListKitListData(private val list: ShadowListKitListView) {
  /*
   * Row keys in data order, the same list the core holds. Section headers and footers included.
   */
  var keys = ArrayList<String>()
    private set

  /*
   * The list reloadData reads the next keys into, swapped with keys after.
   */
  private var spareKeys = ArrayList<String>()

  /*
   * The sections, null without. rowItems has the item of every row, -1 for headers and footers,
   * and rowSeparators whether a separator follows the row. A list without sections maps rows to
   * the same items and makes no call into the core for it.
   */
  var sectionCounts: IntArray? = null
    private set
  var sectionFlags: IntArray? = null
    private set
  var rowItems: IntArray? = null
    private set
  var rowSeparators: BooleanArray? = null
    private set

  /*
   * The section and kind of every row of a list with sections, as ShadowListKitCore.placeOfRow
   * packs them, null without sections.
   */
  var rowPlaces: IntArray? = null
    private set
  var itemCount = 0
    private set

  /*
   * The item keys of a list with sections, in item order. A list without sections uses keys.
   */
  private var sectionItemKeys: List<String>? = null

  /*
   * The changes performBatchUpdates collects and the content versions applyChanges compares
   * live in the list's state, the core's BatchCollector and ContentVersions, which outlive the
   * core. The payload of the last reload in a batch stays here.
   */
  private val state get() = list.state
  private var batchPayload: Any? = null

  /*
   * The two key lists applyChanges diffs, joined for the JNI.
   */
  private val previousPacker = ShadowListKitKeyPacker()
  private val nextPacker = ShadowListKitKeyPacker()

  /*
   * Read the sections and every key into next and return the item keys. A list with sections
   * gets its header and footer rows there, with keys from the core's ListSections.
   */
  private fun readRowKeys(next: ArrayList<String>): List<String> {
    val source = list.dataSource ?: return emptyList()
    next.clear()
    val sections = source as? Sections
    if (sections == null) {
      val count = max(0, source.numberOfItems(list))
      next.ensureCapacity(count)
      for (index in 0 until count) next.add(source.keyForItem(list, index))
      setPlainSections(count)
      return next
    }
    val sectionCount = max(0, sections.numberOfSections(list))
    val counts = IntArray(sectionCount)
    val flags = IntArray(sectionCount)
    val sectionKeys = arrayOfNulls<String>(sectionCount)
    val items = ArrayList<String>()
    for (section in 0 until sectionCount) {
      val count = max(0, sections.numberOfItemsInSection(list, section))
      val hasHeader = sections.titleForHeaderInSection(list, section) != null
      val hasFooter = sections.titleForFooterInSection(list, section) != null
      counts[section] = count
      flags[section] = (if (hasHeader) ShadowListKitCore.SECTION_HEADER else 0) or (if (hasFooter) ShadowListKitCore.SECTION_FOOTER else 0)
      sectionKeys[section] = sections.keyForSection(list, section)
      val first = items.size
      for (local in 0 until count) items.add(source.keyForItem(list, first + local))
    }
    setSections(counts, flags, items)
    // The header and footer keys in row order, placed around each section's items.
    val edges = list.core.edgeRowKeys(sectionKeys, firstItemKeys(counts, items))
    next.ensureCapacity(items.size + edges.size)
    var edge = 0
    var item = 0
    for (section in 0 until sectionCount) {
      if (flags[section] and ShadowListKitCore.SECTION_HEADER != 0) next.add(edges[edge++])
      for (local in 0 until counts[section]) next.add(items[item++])
      if (flags[section] and ShadowListKitCore.SECTION_FOOTER != 0) next.add(edges[edge++])
    }
    readRowItems(next.size)
    return items
  }

  /*
   * The key of each section's first item, empty for a section without items.
   */
  private fun firstItemKeys(counts: IntArray, items: List<String>): Array<String> {
    var first = 0
    return Array(counts.size) { section ->
      val key = if (counts[section] > 0) items[first] else ""
      first += counts[section]
      key
    }
  }

  private fun setPlainSections(count: Int) {
    sectionCounts = null
    sectionFlags = null
    sectionItemKeys = null
    rowItems = null
    rowSeparators = null
    rowPlaces = null
    itemCount = count
    list.liveCore?.setSections(count, null, null)
  }

  private fun setSections(counts: IntArray, flags: IntArray, items: List<String>) {
    sectionCounts = counts
    sectionFlags = flags
    sectionItemKeys = items
    itemCount = items.size
    list.core.setSections(itemCount, counts, flags)
  }

  /*
   * The item of every row, whether a separator follows it and its place, copied once per change.
   */
  private fun readRowItems(rows: Int) {
    val items = IntArray(rows)
    val separators = BooleanArray(rows)
    val places = IntArray(rows)
    list.core.copyRows(items, separators, places)
    rowItems = items
    rowSeparators = separators
    rowPlaces = places
  }

  fun reloadData() {
    if (state.isBatching) {
      state.batchNeedsReload()
      return
    }
    if (list.dataSource == null) return
    val next = spareKeys
    recordContentVersions(readRowKeys(next))
    applyRowKeys(next)
    list.structureChanged()
    list.reloadSectionIndex()
  }

  /*
   * The content version of every item now, which the next applyChanges compares against.
   */
  private fun recordContentVersions(items: List<String>) {
    val versioned = list.dataSource as? ContentVersions ?: return
    state.recordContentVersions(items, contentVersionsOf(versioned, items.size))
  }

  private fun contentVersionsOf(versioned: ContentVersions, count: Int): LongArray =
    LongArray(count) { versioned.contentVersionForItem(list, it) }

  /*
   * The core's keySplice over the key list kept here, which needs no copy of the keys into the
   * core. The core test key_splice_finds_the_changed_middle holds the rule. A caller that already
   * compared the two lists passes the unchanged rows at both ends.
   */
  private fun applyRowKeys(next: ArrayList<String>, knownStart: Int = -1, knownEnd: Int = -1) {
    val start = if (knownStart >= 0) knownStart else commonPrefix(keys, next)
    val end = if (knownEnd >= 0) knownEnd else commonSuffix(keys, next, start)
    if (start < keys.size || start < next.size) {
      if (list.animatesChanges) list.changes.capture(deleted = keys.subList(start, keys.size - end), inserted = next.subList(start, next.size - end))
      list.liveCore?.replaceKeys(start, keys.size - start - end, next, start, next.size - end)
    }
    spareKeys = keys
    keys = next
  }

  fun insertItems(indices: IntArray) {
    if (state.isBatching) {
      state.batchInserted(indices)
      return
    }
    if (list.dataSource is Sections) {
      reloadData()
      return
    }
    val source = list.dataSource ?: return
    // An index past the end inserts at the end. The key is read where the row lands.
    val sorted = ShadowListKitCore.insertionIndices(indices, keys.size)
    if (sorted.isEmpty()) return
    val inserted = Array(sorted.size) { source.keyForItem(list, sorted[it]) }
    if (list.animatesChanges) list.changes.capture(deleted = emptyList(), inserted = inserted.asList())
    forEachRun(sorted) { first, last -> keys.addAll(sorted[first], inserted.asList().subList(first, last + 1)) }
    list.liveCore?.insertKeys(sorted, inserted)
    setPlainSections(keys.size)
    list.structureChanged()
  }

  fun deleteItems(indices: IntArray) {
    if (state.isBatching) {
      state.batchDeleted(indices)
      return
    }
    if (list.dataSource is Sections) {
      reloadData()
      return
    }
    val sorted = ShadowListKitCore.deletionIndices(indices, keys.size)
    if (sorted.isEmpty()) return
    if (list.animatesChanges) list.changes.capture(deleted = sorted.map { keys[it] }, inserted = emptyList())
    // Runs go last to first, which keeps the earlier indices valid.
    val runs = ArrayList<IntArray>()
    forEachRun(sorted) { first, last -> runs.add(intArrayOf(sorted[first], sorted[last])) }
    for (run in runs.asReversed()) keys.subList(run[0], run[1] + 1).clear()
    list.liveCore?.deleteKeys(sorted)
    setPlainSections(keys.size)
    list.structureChanged()
  }

  fun reloadItems(indices: IntArray, payload: Any? = null) {
    if (state.isBatching) {
      state.batchReloaded(indices)
      batchPayload = payload
      return
    }
    val rows = indices.map(list::rowForItem).filter { it >= 0 }.toIntArray()
    reloadRows(rows, payload)
  }

  private fun reloadRows(rows: IntArray, payload: Any?) {
    for (row in rows) {
      val key = keys.getOrNull(row) ?: continue
      val cell = list.mounted[key] ?: continue
      val item = list.itemForRow(row)
      if (payload != null && item >= 0 && list.dataSource?.reconfigureCell(list, cell, item, payload) == true) continue
      list.mounted.remove(key)
      list.recycleCell(cell)
    }
    list.liveCore?.markRemeasure(rows)
    list.structureChanged()
  }

  fun moveItem(sourceIndex: Int, destinationIndex: Int) {
    if (sourceIndex < 0 || destinationIndex < 0) return
    performBatchUpdates({ state.batchMoved(sourceIndex, destinationIndex) })
  }

  fun performBatchUpdates(updates: () -> Unit, completion: ((finished: Boolean) -> Unit)? = null) {
    state.beginBatch()
    var outermost = false
    try {
      updates()
    } finally {
      outermost = state.endBatch()
    }
    if (outermost) commitBatch()
    if (completion == null) return
    val wait = if (list.animatesChanges && list.isAttachedToWindow) (list.itemAnimator as? ShadowListKitDefaultItemAnimator)?.durationMs ?: 0L else 0L
    list.runCommandNow()
    list.postDelayed({ completion(true) }, wait)
  }

  /*
   * Apply the changes a batch collected in one go. The core's batch plan builds the next keys
   * from the keys held and the new ones it reads. A list with sections, or a batch that does
   * not add up, reads everything again.
   */
  private fun commitBatch() {
    val payload = batchPayload
    batchPayload = null
    // The batch's reload flag, then the reloaded items.
    val taken = state.takeBatch() ?: return
    val source = list.dataSource ?: return
    val needsReload = taken[0] != 0
    // The reloaded rows by key, from the data before.
    val reloadedKeys = HashSet<String>()
    for (at in 1 until taken.size) keys.getOrNull(list.rowForItem(taken[at]))?.let(reloadedKeys::add)
    val next = spareKeys
    next.clear()
    var planned = false
    if (source !is Sections && !needsReload) {
      val nextCount = max(0, source.numberOfItems(list))
      val plan = state.planTakenBatch(keys.size, nextCount)
      if (plan != null) {
        next.ensureCapacity(plan.size)
        // The core's keysFromPlan: a kept row takes its key, an inserted one reads its own.
        for ((index, from) in plan.withIndex()) next.add(if (from < 0) source.keyForItem(list, index) else keys[from])
        setPlainSections(next.size)
        planned = true
      } else {
        Log.w("ShadowListKitListView", "batch updates do not add up to $nextCount items, reloading")
      }
    }
    if (!planned) readRowKeys(next)
    commitKeyChanges(next, reloadedKeys, payload, reloadsSectionIndex = !planned)
  }

  /*
   * Take the next keys, then reload the rows of reloadedKeys. Reloaded rows report the change
   * themselves.
   */
  private fun commitKeyChanges(
    next: ArrayList<String>, reloadedKeys: Set<String>, payload: Any?, reloadsSectionIndex: Boolean,
    knownStart: Int = -1, knownEnd: Int = -1,
  ) {
    applyRowKeys(next, knownStart, knownEnd)
    val rows = rowsOfKeys(reloadedKeys)
    if (rows.isNotEmpty()) reloadRows(rows, payload) else list.structureChanged()
    if (reloadsSectionIndex) list.reloadSectionIndex()
  }

  private fun rowsOfKeys(wanted: Set<String>): IntArray {
    if (wanted.isEmpty()) return IntArray(0)
    val rows = ArrayList<Int>()
    for ((row, key) in keys.withIndex()) if (key in wanted) rows.add(row)
    return rows.toIntArray()
  }

  fun applyChanges(): ShadowListKitListChanges {
    // Without a data source readRowKeys leaves the spare list as it was. Nothing is read, the same as reloadData.
    if (list.dataSource == null) return ShadowListKitListChanges(IntArray(0), IntArray(0), IntArray(0), IntArray(0), IntArray(0))
    // readRowKeys fills the spare list. The keys held stay as they are until commitKeyChanges.
    val previousItems = sectionItemKeys ?: keys
    val next = spareKeys
    val nextItems = readRowKeys(next)
    val start = commonPrefix(previousItems, nextItems)
    val end = commonSuffix(previousItems, nextItems, start)
    val diff = ShadowListKitCore.diffKeys(previousItems, nextItems, start, end, previousPacker, nextPacker)

    // Rows that stayed but whose content version changed get reloaded.
    val versioned = list.dataSource as? ContentVersions
    val reloaded =
      if (versioned != null) state.updateContentVersions(nextItems, contentVersionsOf(versioned, nextItems.size)) else IntArray(0)
    val reloadedKeys = HashSet<String>(reloaded.size * 2)
    for (item in reloaded) reloadedKeys.add(nextItems[item])

    // Without sections the items are the rows, and the diff already found their unchanged ends.
    val rowsAreItems = previousItems === keys && nextItems === next
    commitKeyChanges(next, reloadedKeys, null, reloadsSectionIndex = true,
      knownStart = if (rowsAreItems) start else -1, knownEnd = if (rowsAreItems) end else -1)

    var at = 0
    val deleted = IntArray(diff[at]) { diff[at + 1 + it] }
    at += 1 + deleted.size
    val inserted = IntArray(diff[at]) { diff[at + 1 + it] }
    at += 1 + inserted.size
    val moves = diff[at]
    val movedFrom = IntArray(moves) { diff[at + 1 + it * 2] }
    val movedTo = IntArray(moves) { diff[at + 2 + it * 2] }
    return ShadowListKitListChanges(deleted, inserted, movedFrom, movedTo, reloaded)
  }

  /*
   * Calls block with the first and last position of each run of adjacent values in sorted.
   */
  private inline fun forEachRun(sorted: IntArray, block: (first: Int, last: Int) -> Unit) {
    var first = 0
    while (first < sorted.size) {
      var last = first
      while (last + 1 < sorted.size && sorted[last + 1] == sorted[last] + 1) ++last
      block(first, last)
      first = last + 1
    }
  }

  private fun commonPrefix(previous: List<String>, next: List<String>): Int {
    val limit = min(previous.size, next.size)
    var count = 0
    while (count < limit && previous[count] == next[count]) ++count
    return count
  }

  /*
   * Keys equal at the end of both lists, not reaching into the first start keys of either.
   */
  private fun commonSuffix(previous: List<String>, next: List<String>, start: Int): Int {
    val limit = min(previous.size, next.size) - start
    var count = 0
    while (count < limit && previous[previous.size - 1 - count] == next[next.size - 1 - count]) ++count
    return count
  }
}
