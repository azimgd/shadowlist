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
  var itemCount = 0
    private set

  /*
   * The item keys of a list with sections, in item order. A list without sections uses keys.
   */
  private var sectionItemKeys: List<String>? = null

  /*
   * Changes collected by performBatchUpdates until its block returns.
   */
  private var batchDepth = 0
  private val batchDeleted = ArrayList<Int>()
  private val batchInserted = ArrayList<Int>()
  private val batchMovedFrom = ArrayList<Int>()
  private val batchMovedTo = ArrayList<Int>()
  private val batchReloaded = ArrayList<Int>()
  private var batchPayload: Any? = null
  private var batchNeedsReload = false

  /*
   * The content version of every item applyChanges saw, by key.
   */
  private var contentVersions = HashMap<String, Long>()

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
   * The item of every row and whether a separator follows it, copied once per change.
   */
  private fun readRowItems(rows: Int) {
    val items = IntArray(rows)
    val separators = BooleanArray(rows)
    list.core.copyRows(items, separators)
    rowItems = items
    rowSeparators = separators
  }

  fun reloadData() {
    if (batchDepth > 0) {
      batchNeedsReload = true
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
   * The content version of every item now, which the next applyChanges compares against. The
   * versions outlive the core, which is dropped on detach. The rules are the core's
   * ContentVersions, see content_versions_report_items_whose_version_changed.
   */
  private fun recordContentVersions(items: List<String>) {
    val versioned = list.dataSource as? ContentVersions ?: return
    val versions = HashMap<String, Long>(items.size * 2)
    for ((item, key) in items.withIndex()) versions[key] = versioned.contentVersionForItem(list, item)
    contentVersions = versions
  }

  /*
   * The core's keySplice over the key list kept here, which needs no copy of the keys into the
   * core. The core test key_splice_finds_the_changed_middle holds the rule.
   */
  private fun applyRowKeys(next: ArrayList<String>) {
    val start = commonPrefix(keys, next)
    val end = commonSuffix(keys, next, start)
    if (start < keys.size || start < next.size) {
      if (list.animatesChanges) list.changes.capture(removed = keys.subList(start, keys.size - end), inserted = next.subList(start, next.size - end))
      list.liveCore?.replaceKeys(start, keys.size - start - end, next, start, next.size - end)
    }
    spareKeys = keys
    keys = next
  }

  fun insertItems(indices: IntArray) {
    if (batchDepth > 0) {
      batchInserted.addAll(indices.asList())
      return
    }
    if (list.dataSource is Sections) {
      reloadData()
      return
    }
    val source = list.dataSource ?: return
    // An index past the end inserts at the end. The key is read where the row lands.
    val sorted = ShadowListKitCore.insertionPositions(indices, keys.size)
    if (sorted.isEmpty()) return
    val inserted = Array(sorted.size) { source.keyForItem(list, sorted[it]) }
    if (list.animatesChanges) list.changes.capture(removed = emptyList(), inserted = inserted.asList())
    forEachRun(sorted) { first, last -> keys.addAll(sorted[first], inserted.asList().subList(first, last + 1)) }
    list.liveCore?.insertKeys(sorted, inserted)
    setPlainSections(keys.size)
    list.structureChanged()
  }

  fun deleteItems(indices: IntArray) {
    if (batchDepth > 0) {
      batchDeleted.addAll(indices.asList())
      return
    }
    if (list.dataSource is Sections) {
      reloadData()
      return
    }
    val sorted = ShadowListKitCore.deletionPositions(indices, keys.size)
    if (sorted.isEmpty()) return
    if (list.animatesChanges) list.changes.capture(removed = sorted.map { keys[it] }, inserted = emptyList())
    // Runs go last to first, which keeps the earlier indices valid.
    val runs = ArrayList<IntArray>()
    forEachRun(sorted) { first, last -> runs.add(intArrayOf(sorted[first], sorted[last])) }
    for (run in runs.asReversed()) keys.subList(run[0], run[1] + 1).clear()
    list.liveCore?.deleteKeys(sorted)
    setPlainSections(keys.size)
    list.structureChanged()
  }

  fun reloadItems(indices: IntArray, payload: Any? = null) {
    if (batchDepth > 0) {
      batchReloaded.addAll(indices.asList())
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

  fun moveItem(index: Int, newIndex: Int) {
    if (index < 0 || newIndex < 0) return
    performBatchUpdates({
      batchMovedFrom.add(index)
      batchMovedTo.add(newIndex)
    })
  }

  fun performBatchUpdates(updates: () -> Unit, completion: ((finished: Boolean) -> Unit)? = null) {
    ++batchDepth
    try {
      updates()
    } finally {
      --batchDepth
    }
    if (batchDepth == 0) commitBatch()
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
    val deleted = batchDeleted.toIntArray()
    val inserted = batchInserted.toIntArray()
    val movedFrom = batchMovedFrom.toIntArray()
    val movedTo = batchMovedTo.toIntArray()
    val reloaded = batchReloaded.toIntArray()
    val payload = batchPayload
    val needsReload = batchNeedsReload
    batchDeleted.clear()
    batchInserted.clear()
    batchMovedFrom.clear()
    batchMovedTo.clear()
    batchReloaded.clear()
    batchPayload = null
    batchNeedsReload = false
    val source = list.dataSource ?: return
    if (deleted.isEmpty() && inserted.isEmpty() && movedFrom.isEmpty() && reloaded.isEmpty() && !needsReload) return
    // The reloaded rows by key, from the data before.
    val reloadedKeys = HashSet<String>()
    for (item in reloaded) keys.getOrNull(list.rowForItem(item))?.let(reloadedKeys::add)
    val next = spareKeys
    next.clear()
    var planned = false
    if (source !is Sections && !needsReload) {
      val nextCount = max(0, source.numberOfItems(list))
      val plan = ShadowListKitCore.planBatch(keys.size, nextCount, deleted, inserted, movedFrom, movedTo)
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
  private fun commitKeyChanges(next: ArrayList<String>, reloadedKeys: Set<String>, payload: Any?, reloadsSectionIndex: Boolean) {
    applyRowKeys(next)
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
    val previousItems = sectionItemKeys ?: ArrayList(keys)
    val next = spareKeys
    val nextItems = readRowKeys(next)
    val diff = ShadowListKitCore.diffKeys(previousItems, nextItems)

    // Rows that stayed but whose content version changed get reloaded.
    val reloaded = ArrayList<Int>()
    val reloadedKeys = HashSet<String>()
    val versioned = list.dataSource as? ContentVersions
    if (versioned != null) {
      val versions = HashMap<String, Long>(nextItems.size * 2)
      for ((item, key) in nextItems.withIndex()) {
        val version = versioned.contentVersionForItem(list, item)
        val known = contentVersions[key]
        if (known != null && known != version) {
          reloaded.add(item)
          reloadedKeys.add(key)
        }
        versions[key] = version
      }
      contentVersions = versions
    }

    commitKeyChanges(next, reloadedKeys, null, reloadsSectionIndex = true)

    var at = 0
    val deleted = IntArray(diff[at]) { diff[at + 1 + it] }
    at += 1 + deleted.size
    val inserted = IntArray(diff[at]) { diff[at + 1 + it] }
    at += 1 + inserted.size
    val moves = diff[at]
    val movedFrom = IntArray(moves) { diff[at + 1 + it * 2] }
    val movedTo = IntArray(moves) { diff[at + 2 + it * 2] }
    return ShadowListKitListChanges(deleted, inserted, movedFrom, movedTo, reloaded.toIntArray())
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
