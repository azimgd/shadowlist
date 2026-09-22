/*
 * Extras for an indexed ShadowListNative. Rows near the screen get what getExtra returns,
 * merged over their static extra. Rows further away keep only the static extra. The store
 * is patched per field through update, and a removed field is sent as null.
 *
 * No React or binding in here, so it can be tested on its own.
 */

export type ExtraItem = Record<string, unknown>;

export interface ExtraRange {
  start: number;
  end: number;
}

export interface ExtraWindowOptions {
  // Writes a patch into a row's extra, with updateItem on key String(index).
  update: (index: number, patch: ExtraItem) => void;
  // Screens of rows to build on each side of the visible area, at least minPad rows.
  padding: number;
  minPad?: number;
}

const NO_EXTRA: ExtraItem = Object.freeze({}) as ExtraItem;

/*
 * Same value, or arrays with the same entries, so a rebuilt array isn't sent again.
 */
function sameValue(a: unknown, b: unknown): boolean {
  if (a === b) return true;
  if (!Array.isArray(a) || !Array.isArray(b) || a.length !== b.length) {
    return false;
  }
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

export class ExtraWindow {
  private count = 0;
  private statics = new Map<number, ExtraItem>();
  private getExtra: ((index: number) => ExtraItem | null | undefined) | null =
    null;
  // What the store holds for every built row.
  private sent = new Map<number, ExtraItem>();
  private window: ExtraRange;

  constructor(
    private readonly options: ExtraWindowOptions,
    initialWindow: ExtraRange
  ) {
    this.window = initialWindow;
  }

  /*
   * New indexed data. Returns the extras to fill the rows with, static ones everywhere and
   * getExtra ones near the screen. The caller replaces the store, so nothing is patched.
   */
  reset(
    count: number,
    statics: ReadonlyArray<{ index: number; item: ExtraItem }>,
    getExtra: ((index: number) => ExtraItem | null | undefined) | null
  ): { index: number; item: ExtraItem }[] {
    this.count = count;
    this.getExtra = getExtra;
    this.statics = new Map();
    for (const { index, item } of statics) {
      if (index >= 0 && index < count) this.statics.set(index, item);
    }
    this.sent = new Map();
    const seeded: { index: number; item: ExtraItem }[] = [];
    const fill = getExtra === null ? null : this.padded(this.window, 1);
    for (const [index, item] of this.statics) {
      if (fill !== null && index >= fill.start && index <= fill.end) continue;
      seeded.push({ index, item });
    }
    if (fill !== null) {
      for (let index = fill.start; index <= fill.end; index++) {
        const item = this.materialized(index);
        this.sent.set(index, item);
        if (item !== NO_EXTRA) seeded.push({ index, item });
      }
    }
    return seeded;
  }

  /*
   * A new getExtra with the same rows. Rebuild every built row.
   */
  setGetExtra(
    getExtra: ((index: number) => ExtraItem | null | undefined) | null
  ) {
    if (getExtra === this.getExtra) return;
    this.getExtra = getExtra;
    if (getExtra === null) {
      for (const index of [...this.sent.keys()]) this.release(index);
      return;
    }
    this.fill();
    this.refresh();
  }

  /*
   * The visible area moved.
   */
  setWindow(window: ExtraRange) {
    this.window = window;
    if (this.getExtra === null) return;
    const keep = this.padded(window, 2);
    for (const index of [...this.sent.keys()]) {
      if (index < keep.start || index > keep.end) this.release(index);
    }
    this.fill();
  }

  /*
   * Rebuild all built rows, or just the given ones that are built.
   */
  refresh(indices?: Iterable<number>) {
    if (this.getExtra === null) return;
    const targets = indices ?? [...this.sent.keys()];
    for (const index of targets) {
      if (this.sent.has(index)) this.materialize(index);
    }
  }

  /*
   * Built row indices, for tests and debugging.
   */
  materializedIndices(): number[] {
    return [...this.sent.keys()].sort((a, b) => a - b);
  }

  private fill() {
    const fill = this.padded(this.window, 1);
    for (let index = fill.start; index <= fill.end; index++) {
      if (!this.sent.has(index)) this.materialize(index);
    }
  }

  private padded(window: ExtraRange, pads: number): ExtraRange {
    const size = Math.max(0, window.end - window.start + 1);
    const pad =
      Math.max(this.options.minPad ?? 20, size * this.options.padding) * pads;
    return {
      start: Math.max(0, window.start - pad),
      end: Math.min(this.count - 1, window.end + pad),
    };
  }

  private staticItem(index: number): ExtraItem {
    return this.statics.get(index) ?? NO_EXTRA;
  }

  private materialized(index: number): ExtraItem {
    const extra = this.getExtra?.(index);
    const base = this.staticItem(index);
    if (extra === null || extra === undefined) return base;
    return base === NO_EXTRA ? extra : { ...base, ...extra };
  }

  private materialize(index: number) {
    if (index < 0 || index >= this.count) return;
    const previous = this.sent.get(index) ?? this.staticItem(index);
    const next = this.materialized(index);
    this.sent.set(index, next);
    this.send(index, previous, next);
  }

  private release(index: number) {
    const previous = this.sent.get(index);
    this.sent.delete(index);
    if (previous === undefined || index >= this.count) return;
    this.send(index, previous, this.staticItem(index));
  }

  private send(index: number, previous: ExtraItem, next: ExtraItem) {
    if (previous === next) return;
    let patch: ExtraItem | null = null;
    for (const key in next) {
      if (!sameValue(next[key], previous[key])) {
        patch ??= {};
        patch[key] = next[key];
      }
    }
    for (const key in previous) {
      if (!(key in next)) {
        patch ??= {};
        patch[key] = null;
      }
    }
    if (patch !== null) this.options.update(index, patch);
  }
}
