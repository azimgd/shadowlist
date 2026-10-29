import {
  ShadowListKitCore,
  ShadowListKitListState,
  isShadowListKitLoaded,
  unchangedEnds,
} from './ShadowListKitCore.js';
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
   * The changes performBatchUpdates collects and the content versions applyChanges compares,
   * the core's BatchCollector and ContentVersions, which outlive the core. The payload of the
   * last reload in a batch stays here.
   */
  private readonly state = new ShadowListKitListState();
  private batchPayload: unknown = null;

  /*
   * A batch that started before the core loaded. Its changes are dropped because nothing is
   * laid out yet, and it reads everything again when its outermost block returns.
   */
  private unloadedBatch = false;

  /*
   * Whether changes go to the batch being collected instead of the data.
   */
  private get batching(): boolean {
    return this.unloadedBatch || this.state.isBatching;
  }

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
    if (this.batching) {
      if (!this.unloadedBatch) this.state.batchNeedsReload();
      return;
    }
    if (!this.list.dataSource) return;
    const next = this.readKeys();
    this.recordContentVersions(next);
    this.applyKeys(next);
    this.list.structureChanged();
  }

  /*
   * The content version of every item now, which the next applyChanges compares against.
   */
  private recordContentVersions(items: ReadonlyArray<string>): void {
    const versions = this.contentVersionsOf(items.length);
    if (versions) this.state.recordContentVersions(items, versions);
  }

  private contentVersionsOf(count: number): (number | string)[] | null {
    const source = this.list.dataSource;
    if (!source?.contentVersionForItem) return null;
    const versions = new Array<number | string>(count);
    for (let item = 0; item < count; ++item) {
      versions[item] = source.contentVersionForItem(this.list, item);
    }
    return versions;
  }

  /*
   * The core's keySplice over the key list kept here, which needs no copy of the keys into the
   * core. The core test key_splice_finds_the_changed_middle holds the rule. A caller that already
   * compared the two lists passes how many keys match at each end.
   */
  private applyKeys(
    next: string[],
    unchanged: { start: number; end: number } = unchangedEnds(this.keys, next)
  ): void {
    const { start, end } = unchanged;
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
    if (this.batching) {
      if (!this.unloadedBatch) this.state.batchInserted(indices);
      return;
    }
    const source = this.list.dataSource;
    if (!source) return;
    // An index past the end inserts at the end. The key is read where the row lands.
    const sorted = Array.from(
      ShadowListKitCore.insertionIndices(indices, this.keys.length)
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
    if (this.batching) {
      if (!this.unloadedBatch) this.state.batchDeleted(indices);
      return;
    }
    const sorted = Array.from(
      ShadowListKitCore.deletionIndices(indices, this.keys.length)
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
    if (this.batching) {
      if (!this.unloadedBatch) this.state.batchReloaded(indices);
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

  moveItem(sourceIndex: number, destinationIndex: number): void {
    if (sourceIndex < 0 || destinationIndex < 0) return;
    this.performBatchUpdates(() => {
      if (!this.unloadedBatch)
        this.state.batchMoved(sourceIndex, destinationIndex);
    });
  }

  performBatchUpdates(
    updates: () => void,
    completion?: (finished: boolean) => void
  ): void {
    if (this.unloadedBatch || !isShadowListKitLoaded()) {
      this.performUnloadedBatch(updates);
    } else {
      this.state.beginBatch();
      let outermost = false;
      try {
        updates();
      } finally {
        outermost = this.state.endBatch();
      }
      if (outermost) this.commitBatch();
    }
    if (!completion) return;
    this.list.runCommandNow();
    completion(true);
  }

  private performUnloadedBatch(updates: () => void): void {
    const outermost = !this.unloadedBatch;
    this.unloadedBatch = true;
    try {
      updates();
    } finally {
      if (outermost) this.unloadedBatch = false;
    }
    if (!outermost) return;
    this.batchPayload = null;
    this.reloadData();
  }

  /*
   * Apply the changes a batch collected in one go. The core's batch plan builds the next keys
   * from the keys held and the new ones it reads. A batch that does not add up reads
   * everything again.
   */
  private commitBatch(): void {
    const payload = this.batchPayload;
    this.batchPayload = null;
    // The batch's reload flag, then the reloaded items.
    const taken = this.state.takeBatch();
    const source = this.list.dataSource;
    if (!taken || !source) return;
    const needsReload = taken[0] !== 0;
    // The reloaded rows by key, from the data before.
    const reloadedKeys = new Set<string>();
    for (let at = 1; at < taken.length; ++at) {
      const key = this.keys[taken[at]!];
      if (key !== undefined) reloadedKeys.add(key);
    }
    let next: string[] | null = null;
    if (!needsReload) {
      const nextCount = Math.max(0, source.numberOfItems(this.list));
      const plan = this.state.planTakenBatch(this.keys.length, nextCount);
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
    payload: unknown,
    unchanged?: { start: number; end: number }
  ): void {
    this.applyKeys(next, unchanged);
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
    const unchanged = unchangedEnds(previous, next);
    const diff = ShadowListKitCore.diffKeys(previous, next, unchanged);

    // Rows that stayed but whose content version changed get reloaded.
    const versions = this.contentVersionsOf(next.length);
    const reloaded = versions
      ? Array.from(this.state.updateContentVersions(next, versions))
      : [];
    const reloadedKeys = new Set(reloaded.map((item) => next[item]!));

    this.commitKeyChanges(next, reloadedKeys, null, unchanged);

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
