import Foundation
import os

/*
 * Row layouts by row key, filled ahead on a background queue. A layout is kept for the reuse
 * identifier, the width and the content version it was made for, and made again when one
 * differs. Every method is thread safe.
 */
public final class ShadowListUtilsLayoutCache {
  private struct Entry {
    let reuseIdentifier: String
    let width: CGFloat
    let contentVersion: Int
    let layout: ShadowListUtilsRowLayout
  }

  private struct State {
    var entries: [String: Entry] = [:]
    var generation = 0

    /*
     * Counts the times layouts were dropped. A layout that was being made on another thread
     * while they were is not kept: what it was made from, like the text size, may be gone.
     */
    var invalidations = 0
  }

  /*
   * How many rows on each side of the given index one prefetch lays out.
   */
  static let prefetchRadius = 250

  private let state = OSAllocatedUnfairLock(uncheckedState: State())
  private let queue = DispatchQueue(label: "com.shadowlist.utils.layout", qos: .userInitiated)

  public init() {}

  /*
   * The row's layout for a width, from the cache or computed now.
   */
  public func layout(_ row: ShadowListUtilsRow, width: CGFloat) -> ShadowListUtilsRowLayout {
    let key = row.key
    let reuseIdentifier = row.reuseIdentifier
    let contentVersion = row.contentVersion
    let (found, started) = state.withLockUnchecked { state -> (ShadowListUtilsRowLayout?, Int) in
      guard let entry = state.entries[key], entry.reuseIdentifier == reuseIdentifier, entry.width == width,
        entry.contentVersion == contentVersion
      else {
        return (nil, state.invalidations)
      }
      return (entry.layout, state.invalidations)
    }
    if let found {
      return found
    }
    let layout = row.layout(width: width)
    state.withLockUnchecked { state in
      guard state.invalidations == started else { return }
      state.entries[key] = Entry(reuseIdentifier: reuseIdentifier, width: width, contentVersion: contentVersion, layout: layout)
    }
    return layout
  }

  /*
   * Lay out rows the list has not reached yet, off the main thread: the row at the given index
   * and up to prefetchRadius rows on each side of it, nearest first. A newer call replaces the
   * one still running.
   */
  public func prefetch(_ rows: [ShadowListUtilsRow], width: CGFloat, around index: Int = 0) {
    guard width > 0, !rows.isEmpty else { return }
    let generation = state.withLockUnchecked { state -> Int in
      state.generation += 1
      return state.generation
    }
    let center = min(max(index, 0), rows.count - 1)
    queue.async { [weak self] in
      // Outward from the center: the row there, then one further on each side.
      for distance in 0...Self.prefetchRadius {
        if center + distance >= rows.count, center - distance < 0 { return }
        for position in distance == 0 ? [center] : [center + distance, center - distance] where rows.indices.contains(position) {
          guard let self, self.state.withLockUnchecked({ $0.generation == generation }) else { return }
          _ = self.layout(rows[position], width: width)
        }
      }
    }
  }

  public func invalidate(_ key: String) {
    state.withLockUnchecked { state in
      state.invalidations += 1
      state.entries.removeValue(forKey: key)
    }
  }

  public func invalidateAll() {
    state.withLockUnchecked { state in
      state.generation += 1
      state.invalidations += 1
      state.entries.removeAll()
    }
  }

  /*
   * Drop the layouts of keys that left the data, and end the prefetch still running over the
   * rows before. A list calls it with the keys of the rows it shows now.
   */
  func prune(keeping keys: Set<String>) {
    state.withLockUnchecked { state in
      state.generation += 1
      state.invalidations += 1
      state.entries = state.entries.filter { keys.contains($0.key) }
    }
  }

  /*
   * End the prefetch still running, like for a list that left its window.
   */
  func cancelPrefetch() {
    state.withLockUnchecked { $0.generation += 1 }
  }

  /*
   * How many layouts the cache holds.
   */
  var count: Int {
    state.withLockUnchecked { $0.entries.count }
  }
}
