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
 * list's state holds in wasm and hands to the core. Reloads, inserts, deletes, batches and
 * applyChanges send the state only what changed and report the change to the list, which mounts
 * from the keys here.
 */
export class ShadowListKitListData {
  /*
   * Row keys in data order, the same list the state and the core hold.
   */
  keys: string[] = [];

  /*
   * The keys held in wasm, the changes performBatchUpdates collects and the content versions
   * applyChanges compares live in the list's state, which outlives the core. The payload of the
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
    // The content version of every item now, which the next applyChanges compares against.
    this.commitKeys(next, { record: this.contentVersionsOf(next.length) });
    this.list.structureChanged();
  }

  /*
   * A core made after the keys: the state gives it every key it holds.
   */
  seedCore(core: ShadowListKitCore): void {
    if (this.keys.length > 0) this.state.seedCore(core, this.keys);
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
   * Take next as the keys. The core's keySplice over the key list kept here finds the unchanged
   * rows at both ends, and only the keys between them cross into the state. The core test
   * key_splice_finds_the_changed_middle holds the rule. Returns what
   * ShadowListKitListState.commitKeys returns.
   */
  private commitKeys(
    next: string[],
    change: Parameters<ShadowListKitListState['commitKeys']>[4]
  ): Int32Array | null {
    const committed = this.state.commitKeys(
      this.list.liveCore,
      this.keys,
      next,
      unchangedEnds(this.keys, next),
      change
    );
    this.keys = next;
    return committed;
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
    this.state.insertRows(this.list.liveCore, this.keys, sorted, inserted);
    forEachRun(sorted, (first, last) =>
      this.keys.splice(sorted[first]!, 0, ...inserted.slice(first, last + 1))
    );
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
    this.state.deleteRows(this.list.liveCore, this.keys, sorted);
    // Runs go last to first, which keeps the earlier indices valid.
    const runs: [number, number][] = [];
    forEachRun(sorted, (first, last) =>
      runs.push([sorted[first]!, sorted[last]!])
    );
    for (const [low, high] of runs.reverse()) {
      this.keys.splice(low, high - low + 1);
    }
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
    this.commitKeys(next ?? this.readKeys(), {});
    this.reloadKeyRows(reloadedKeys, payload);
  }

  /*
   * After the next keys are taken, reload the rows of reloadedKeys. Reloaded rows report the
   * change themselves.
   */
  private reloadKeyRows(
    reloadedKeys: ReadonlySet<string>,
    payload: unknown
  ): void {
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
    const next = this.readKeys();
    // The state diffs the keys and finds the items whose content version changed under the same key.
    const diff = this.commitKeys(next, {
      diff: true,
      update: this.contentVersionsOf(next.length),
    })!;

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
    at += 1 + moves * 2;
    const reloaded = Array.from(diff.subarray(at + 1, at + 1 + diff[at]!));

    // Rows that stayed but whose content version changed get reloaded.
    this.reloadKeyRows(new Set(reloaded.map((item) => next[item]!)), null);
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
