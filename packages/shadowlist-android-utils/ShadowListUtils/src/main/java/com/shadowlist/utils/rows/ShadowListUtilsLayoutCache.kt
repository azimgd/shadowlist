package com.shadowlist.utils

import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger

/*
 * Row layouts by row key, filled ahead on a background thread. A layout is kept for the reuse
 * identifier, the width and the content version it was made for, and made again when one
 * differs. Every method is thread safe.
 */
class ShadowListUtilsLayoutCache {
  private class Entry(
    val reuseIdentifier: String,
    val widthPx: Int,
    val contentVersion: Int,
    val layout: ShadowListUtilsRowLayout,
  )

  private val entries = ConcurrentHashMap<String, Entry>()
  private val generation = AtomicInteger()

  /*
   * Counts the times layouts were dropped. A layout that was being made on another thread
   * while they were is not kept: what it was made from, like the text size, may be gone.
   */
  private var invalidations = 0
  private val lock = Any()

  /*
   * The row's layout for a width, from the cache or computed now.
   */
  fun layout(row: ShadowListUtilsRow, widthPx: Int): ShadowListUtilsRowLayout {
    cached(row, widthPx)?.let { return it }
    val contentVersion = row.contentVersion
    val started = synchronized(lock) { invalidations }
    val layout = row.layout(widthPx)
    synchronized(lock) {
      if (invalidations == started) entries[row.key] = Entry(row.reuseIdentifier, widthPx, contentVersion, layout)
    }
    return layout
  }

  private fun cached(row: ShadowListUtilsRow, widthPx: Int): ShadowListUtilsRowLayout? {
    val entry = entries[row.key] ?: return null
    val current = entry.widthPx == widthPx && entry.contentVersion == row.contentVersion &&
      entry.reuseIdentifier == row.reuseIdentifier
    return if (current) entry.layout else null
  }

  /*
   * Lay out rows the list has not reached yet, off the UI thread: the row at the given index
   * and up to PREFETCH_RADIUS rows on each side of it, nearest first. A newer call replaces the
   * one still running.
   */
  fun prefetch(rows: List<ShadowListUtilsRow>, widthPx: Int, around: Int = 0) {
    if (widthPx <= 0 || rows.isEmpty()) return
    val current = generation.incrementAndGet()
    val center = around.coerceIn(0, rows.size - 1)
    executor.execute {
      // Outward from the center: the row there, then one further on each side.
      for (distance in 0..PREFETCH_RADIUS) {
        val after = center + distance
        val before = center - distance
        if (after >= rows.size && before < 0) return@execute
        for (position in if (distance == 0) intArrayOf(center) else intArrayOf(after, before)) {
          if (position !in rows.indices) continue
          if (generation.get() != current) return@execute
          layout(rows[position], widthPx)
        }
      }
    }
  }

  fun invalidate(key: String) {
    synchronized(lock) {
      ++invalidations
      entries.remove(key)
    }
  }

  fun invalidateAll() {
    cancelPrefetch()
    synchronized(lock) {
      ++invalidations
      entries.clear()
    }
  }

  /*
   * Drop the layouts of keys that left the data, and end the prefetch still running over the
   * rows before. A list calls it with the keys of the rows it shows now.
   */
  internal fun prune(keys: Set<String>) {
    cancelPrefetch()
    synchronized(lock) {
      ++invalidations
      entries.keys.retainAll(keys)
    }
  }

  /*
   * End the prefetch still running, like for a list that left its window.
   */
  internal fun cancelPrefetch() {
    generation.incrementAndGet()
  }

  /*
   * How many layouts the cache holds.
   */
  internal val count: Int get() = entries.size

  private companion object {
    /*
     * How many rows on each side of the given index one prefetch lays out.
     */
    const val PREFETCH_RADIUS = 250

    /*
     * Every cache shares these threads. They end with the process and hold no list.
     */
    val executor = Executors.newFixedThreadPool(2) { work ->
      Thread(work, "ShadowListUtilsLayout").apply {
        isDaemon = true
        priority = Thread.NORM_PRIORITY - 1
      }
    }
  }
}
