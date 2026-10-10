import { ShadowListKitCore } from './ShadowListKitCore.js';
import { ShadowListKitListChanges } from './ShadowListKitListChanges.js';
import type { ShadowListKitListView } from './ShadowListKitListView.js';

/*
 * The list's data: row keys and the changes that move them. The keys are the same list the
 * core holds. Reloads, inserts, deletes, batches and applyChanges update both and report the
 * change to the list, which mounts from the keys here.
 */
export class ShadowListKitListData {
  /*
   * Row keys in data order, the same list the core holds.
   */
  keys: string[] = [];

  /*
   * Changes collected by performBatchUpdates until its block returns.
   */
  private batchDepth = 0;
  private batchDeleted: number[] = [];
  private batchInserted: number[] = [];
  private batchMovedFrom: number[] = [];
  private batchMovedTo: number[] = [];
  private batchReloaded: number[] = [];
  private batchPayload: unknown = null;
  private batchNeedsReload = false;

  /*
   * The content version of every item applyChanges saw, by key.
   */
  private contentVersions = new Map<string, number | string>();

  constructor(private readonly list: ShadowListKitListView) {}

  get itemCount(): number {
    return this.keys.length;
  }

  /*
   * Every key the data source has now.
   */
  private readKeys(): string[] {
    const source = this.list.dataSource;
    if (!source) return [];
    const count = Math.max(0, source.numberOfItems(this.list));
    const next = new Array<string>(count);
    for (let index = 0; index < count; ++index) {
      next[index] = source.keyForItem(this.list, index);
    }
    return next;
  }

  reloadData(): void {
    if (this.batchDepth > 0) {
      this.batchNeedsReload = true;
      return;
    }
    if (!this.list.dataSource) return;
    const next = this.readKeys();
    this.recordContentVersions(next);
    this.applyKeys(next);
    this.list.structureChanged();
  }

  /*
   * The content version of every item now, which the next applyChanges compares against. The
   * rules are the core's ContentVersions, see content_versions_report_items_whose_version_changed.
   */
  private recordContentVersions(items: ReadonlyArray<string>): void {
    const source = this.list.dataSource;
    if (!source?.contentVersionForItem) return;
    const versions = new Map<string, number | string>();
    items.forEach((key, item) =>
      versions.set(key, source.contentVersionForItem!(this.list, item))
    );
    this.contentVersions = versions;
  }

  /*
   * The core's keySplice over the key list kept here, which needs no copy of the keys into the
   * core. The core test key_splice_finds_the_changed_middle holds the rule.
   */
  private applyKeys(next: string[]): void {
    const start = commonPrefix(this.keys, next);
    const end = commonSuffix(this.keys, next, start);
    if (start < this.keys.length || start < next.length) {
      this.list.liveCore?.replaceKeys(
        start,
        this.keys.length - start - end,
        next,
        start,
        next.length - end
      );
    }
    this.keys = next;
  }

  insertItems(indices: ReadonlyArray<number>): void {
    if (this.batchDepth > 0) {
      this.batchInserted.push(...indices);
      return;
    }
    const source = this.list.dataSource;
    if (!source) return;
    // An index past the end inserts at the end. The key is read where the row lands.
    const sorted = Array.from(
      ShadowListKitCore.insertionPositions(indices, this.keys.length)
    );
    if (sorted.length === 0) return;
    const inserted = sorted.map((index) => source.keyForItem(this.list, index));
    forEachRun(sorted, (first, last) =>
      this.keys.splice(sorted[first]!, 0, ...inserted.slice(first, last + 1))
    );
    this.list.liveCore?.insertKeys(sorted, inserted);
    this.list.structureChanged();
  }

  deleteItems(indices: ReadonlyArray<number>): void {
    if (this.batchDepth > 0) {
      this.batchDeleted.push(...indices);
      return;
    }
    const sorted = Array.from(
      ShadowListKitCore.deletionPositions(indices, this.keys.length)
    );
    if (sorted.length === 0) return;
    // Runs go last to first, which keeps the earlier indices valid.
    const runs: [number, number][] = [];
    forEachRun(sorted, (first, last) =>
      runs.push([sorted[first]!, sorted[last]!])
    );
    for (const [low, high] of runs.reverse()) {
      this.keys.splice(low, high - low + 1);
    }
    this.list.liveCore?.deleteKeys(sorted);
    this.list.structureChanged();
  }

  reloadItems(indices: ReadonlyArray<number>, payload: unknown = null): void {
    if (this.batchDepth > 0) {
      this.batchReloaded.push(...indices);
      this.batchPayload = payload;
      return;
    }
    this.reloadRows(
      indices.filter((index) => index >= 0 && index < this.keys.length),
      payload
    );
  }

  private reloadRows(rows: ReadonlyArray<number>, payload: unknown): void {
    for (const row of rows) {
      const key = this.keys[row];
      if (key === undefined) continue;
      const cell = this.list.mounted.get(key);
      if (!cell) continue;
      if (
        payload != null &&
        this.list.dataSource?.reconfigureCell?.(this.list, cell, row, payload)
      ) {
        continue;
      }
      this.list.mounted.delete(key);
      this.list.recycleCell(cell);
    }
    this.list.liveCore?.markRemeasure(rows);
    this.list.structureChanged();
  }

  moveItem(index: number, newIndex: number): void {
    if (index < 0 || newIndex < 0) return;
    this.performBatchUpdates(() => {
      this.batchMovedFrom.push(index);
      this.batchMovedTo.push(newIndex);
    });
  }

  performBatchUpdates(updates: () => void, completion?: () => void): void {
    ++this.batchDepth;
    try {
      updates();
    } finally {
      --this.batchDepth;
    }
    if (this.batchDepth === 0) this.commitBatch();
    if (!completion) return;
    this.list.runCommandNow();
    completion();
  }

  /*
   * Apply the changes a batch collected in one go. The core's batch plan builds the next keys
   * from the keys held and the new ones it reads. A batch that does not add up reads
   * everything again.
   */
  private commitBatch(): void {
    const deleted = this.batchDeleted;
    const inserted = this.batchInserted;
    const movedFrom = this.batchMovedFrom;
    const movedTo = this.batchMovedTo;
    const reloaded = this.batchReloaded;
    const payload = this.batchPayload;
    const needsReload = this.batchNeedsReload;
    this.batchDeleted = [];
    this.batchInserted = [];
    this.batchMovedFrom = [];
    this.batchMovedTo = [];
    this.batchReloaded = [];
    this.batchPayload = null;
    this.batchNeedsReload = false;
    const source = this.list.dataSource;
    if (!source) return;
    if (
      deleted.length === 0 &&
      inserted.length === 0 &&
      movedFrom.length === 0 &&
      reloaded.length === 0 &&
      !needsReload
    ) {
      return;
    }
    // The reloaded rows by key, from the data before.
    const reloadedKeys = new Set<string>();
    for (const item of reloaded) {
      const key = this.keys[item];
      if (key !== undefined) reloadedKeys.add(key);
    }
    let next: string[] | null = null;
    if (!needsReload) {
      const nextCount = Math.max(0, source.numberOfItems(this.list));
      const plan = ShadowListKitCore.planBatch(
        this.keys.length,
        nextCount,
        deleted,
        inserted,
        movedFrom,
        movedTo
      );
      if (plan) {
        // The core's keysFromPlan: a kept row takes its key, an inserted one reads its own.
        next = Array.from(plan, (from, index) =>
          from < 0 ? source.keyForItem(this.list, index) : this.keys[from]!
        );
      } else {
        console.warn(
          `ShadowListKitListView: batch updates do not add up to ${nextCount} items, reloading`
        );
      }
    }
    this.commitKeyChanges(next ?? this.readKeys(), reloadedKeys, payload);
  }

  /*
   * Take the next keys, then reload the rows of reloadedKeys. Reloaded rows report the change
   * themselves.
   */
  private commitKeyChanges(
    next: string[],
    reloadedKeys: ReadonlySet<string>,
    payload: unknown
  ): void {
    this.applyKeys(next);
    const rows = this.rowsOfKeys(reloadedKeys);
    if (rows.length > 0) this.reloadRows(rows, payload);
    else this.list.structureChanged();
  }

  private rowsOfKeys(wanted: ReadonlySet<string>): number[] {
    const rows: number[] = [];
    if (wanted.size === 0) return rows;
    this.keys.forEach((key, row) => {
      if (wanted.has(key)) rows.push(row);
    });
    return rows;
  }

  applyChanges(): ShadowListKitListChanges {
    const source = this.list.dataSource;
    if (!source) return new ShadowListKitListChanges([], [], [], [], []);
    const previous = this.keys;
    const next = this.readKeys();
    const diff = ShadowListKitCore.diffKeys(previous, next);

    // Rows that stayed but whose content version changed get reloaded.
    const reloaded: number[] = [];
    const reloadedKeys = new Set<string>();
    if (source.contentVersionForItem) {
      const versions = new Map<string, number | string>();
      next.forEach((key, item) => {
        const version = source.contentVersionForItem!(this.list, item);
        const known = this.contentVersions.get(key);
        if (known !== undefined && known !== version) {
          reloaded.push(item);
          reloadedKeys.add(key);
        }
        versions.set(key, version);
      });
      this.contentVersions = versions;
    }

    this.commitKeyChanges(next, reloadedKeys, null);

    let at = 0;
    const deleted = Array.from(diff.subarray(at + 1, at + 1 + diff[at]!));
    at += 1 + deleted.length;
    const inserted = Array.from(diff.subarray(at + 1, at + 1 + diff[at]!));
    at += 1 + inserted.length;
    const moves = diff[at]!;
    const movedFrom: number[] = [];
    const movedTo: number[] = [];
    for (let move = 0; move < moves; ++move) {
      movedFrom.push(diff[at + 1 + move * 2]!);
      movedTo.push(diff[at + 2 + move * 2]!);
    }
    return new ShadowListKitListChanges(
      deleted,
      inserted,
      movedFrom,
      movedTo,
      reloaded
    );
  }
}

/*
 * Calls block with the first and last position of each run of adjacent values in sorted.
 */
function forEachRun(
  sorted: ReadonlyArray<number>,
  block: (first: number, last: number) => void
): void {
  let first = 0;
  while (first < sorted.length) {
    let last = first;
    while (last + 1 < sorted.length && sorted[last + 1] === sorted[last]! + 1) {
      ++last;
    }
    block(first, last);
    first = last + 1;
  }
}

function commonPrefix(
  previous: ReadonlyArray<string>,
  next: ReadonlyArray<string>
): number {
  const limit = Math.min(previous.length, next.length);
  let count = 0;
  while (count < limit && previous[count] === next[count]) ++count;
  return count;
}

/*
 * Keys equal at the end of both lists, not reaching into the first start keys of either.
 */
function commonSuffix(
  previous: ReadonlyArray<string>,
  next: ReadonlyArray<string>,
  start: number
): number {
  const limit = Math.min(previous.length, next.length) - start;
  let count = 0;
  while (
    count < limit &&
    previous[previous.length - 1 - count] === next[next.length - 1 - count]
  ) {
    ++count;
  }
  return count;
}
