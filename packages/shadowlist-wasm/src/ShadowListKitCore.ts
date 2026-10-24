import createShadowListKitModule, {
  type ShadowListKitModule,
} from './shadowlistkit.mjs';

let wasmModule: ShadowListKitModule | null = null;
let loading: Promise<void> | null = null;

/*
 * The core's OFFSET_MOVED_THRESHOLD, read once on load. A scroll frame makes no wasm call.
 */
let offsetMovedThreshold = 0;

/*
 * The cores by handle, for the measure calls the wasm side makes. Weak, which lets a list
 * nobody holds be collected with its core.
 */
const cores = new Map<number, WeakRef<ShadowListKitCore>>();

/*
 * Frees the peer of a core whose list was dropped without destroy(), like one never connected.
 */
const reclaimer = new FinalizationRegistry<number>((handle) => {
  cores.delete(handle);
  wasmModule?._ShadowListKitCore_nativeDestroy(handle);
});

/*
 * Frees the peer of a list state once its list is collected. A list has no call that ends it.
 */
const stateReclaimer = new FinalizationRegistry<number>((handle) => {
  wasmModule?._ShadowListKitListState_nativeDestroy(handle);
});

/*
 * A heap area every call writes its inputs to and reads its outputs from. Grown as needed and
 * reused. The wasm side copies inputs when a call starts.
 */
let scratch = 0;
let scratchBytes = 0;

const encoder = new TextEncoder();
const decoder = new TextDecoder();

/*
 * Load the shadowlist core. Lists need it before they lay out. locateFile finds
 * shadowlistkit.wasm when it is not served next to shadowlistkit.mjs.
 */
export function loadShadowListKit(options?: {
  locateFile?: (path: string, scriptDirectory: string) => string;
}): Promise<void> {
  loading ??= createShadowListKitModule(options).then((loaded) => {
    loaded.shadowListKitMeasureRow = (handle, row, crossSize) =>
      cores.get(handle)?.deref()?.measureRow(row, crossSize) ?? 0;
    offsetMovedThreshold =
      loaded._ShadowListKitCore_nativeOffsetMovedThreshold();
    wasmModule = loaded;
  });
  return loading;
}

export function isShadowListKitLoaded(): boolean {
  return wasmModule != null;
}

function wasm(): ShadowListKitModule {
  if (!wasmModule) {
    throw new Error('Call loadShadowListKit() before a list lays out');
  }
  return wasmModule;
}

/*
 * The scratch area with room for bytes, 8 byte aligned.
 */
function reserve(bytes: number): number {
  if (bytes <= scratchBytes) return scratch;
  const loaded = wasm();
  if (scratch !== 0) loaded._free(scratch);
  scratchBytes = Math.max(bytes, scratchBytes * 2, 1024);
  scratch = loaded._malloc(scratchBytes);
  return scratch;
}

function align(bytes: number): number {
  return Math.ceil(bytes / 8) * 8;
}

function putInts(at: number, values: ArrayLike<number>): void {
  wasm().HEAP32.set(values, at / 4);
}

/*
 * The int result at pointer, its length first, copied out.
 */
function takeInts(pointer: number): Int32Array {
  const heap = wasm().HEAP32;
  const start = pointer / 4 + 1;
  return heap.slice(start, start + (heap[pointer / 4] ?? 0));
}

/*
 * The bytes keys from until to need at most when packed: their ends, then UTF-8 of at most
 * three bytes per UTF-16 unit.
 */
function packedBytes(
  keys: ReadonlyArray<string>,
  from: number,
  to: number
): number {
  let length = 0;
  for (let position = from; position < to; ++position) {
    length += keys[position]!.length;
  }
  return align((to - from) * 4) + length * 3;
}

/*
 * Keys from until to packed at at: where each key ends, then the keys joined as UTF-8. Two
 * heap regions instead of one string per key. Returns the pointers and the bytes taken.
 */
function pack(
  keys: ReadonlyArray<string>,
  from: number,
  to: number,
  at: number
): { ends: number; packed: number; taken: number } {
  const count = Math.max(0, to - from);
  const ends = at;
  const packed = at + align(count * 4);
  const loaded = wasm();
  const bytes = loaded.HEAPU8;
  const heap = loaded.HEAP32;
  let written = 0;
  for (let position = from; position < to; ++position) {
    const key = keys[position]!;
    written = putAscii(key, bytes, packed + written)
      ? written + key.length
      : written +
        encoder.encodeInto(
          key,
          bytes.subarray(packed + written, packed + written + key.length * 3)
        ).written;
    heap[ends / 4 + position - from] = written;
  }
  return { ends, packed, taken: align(packed - at + written) };
}

/*
 * Write an ASCII key byte by byte. Returns false at the first other character, which leaves
 * the key to TextEncoder. Keys are mostly ASCII ids, and one TextEncoder call per key costs
 * more than the copy.
 */
function putAscii(key: string, bytes: Uint8Array, at: number): boolean {
  for (let index = 0; index < key.length; ++index) {
    const unit = key.charCodeAt(index);
    if (unit >= 0x80) return false;
    bytes[at + index] = unit;
  }
  return true;
}

/*
 * One key as UTF-8 at at. Returns its length.
 */
function putKey(key: string, at: number): number {
  const bytes = wasm().HEAPU8;
  return encoder.encodeInto(key, bytes.subarray(at, at + key.length * 3))
    .written;
}

/*
 * The shadowlist core behind one list, through the core's ListDriver in C++ compiled to wasm.
 * Every call runs on the main thread. Pass values go in and out through an array this object
 * reuses.
 */
export class ShadowListKitCore {
  static readonly PASS_OFFSET = 0;
  static readonly PASS_WINDOW_ALONG = 1;
  static readonly PASS_WINDOW_CROSS = 2;
  static readonly PASS_HEADER_SIZE = 3;
  static readonly PASS_FOOTER_SIZE = 4;
  static readonly PASS_PHASE = 5;
  static readonly PASS_USER_SCROLLED = 6;
  static readonly PASS_TRACKING = 7;
  static readonly PASS_OUT_OFFSET = 8;
  static readonly PASS_OUT_CONTENT = 9;
  static readonly PASS_OUT_BAND_LOW = 10;
  static readonly PASS_OUT_BAND_HIGH = 11;
  static readonly PASS_OUT_SETTLING = 12;
  static readonly PASS_OUT_REACHED_START = 13;
  static readonly PASS_OUT_REACHED_END = 14;
  static readonly PASS_OUT_GEOMETRY = 15;
  static readonly PASS_OUT_MEASURED_LOW = 16;
  static readonly PASS_OUT_MEASURED_HIGH = 17;
  static readonly PASS_SLOTS = 18;

  /*
   * Values of PASS_PHASE, the same as the core's ScrollPhase.
   */
  static readonly PHASE_IDLE = 0;
  static readonly PHASE_DRAGGING = 1;
  static readonly PHASE_SETTLING = 2;

  /*
   * Where an animated scroll command lands, the same as the core's ScrollLanding::Target.
   */
  static readonly LANDING_ROW = 1;
  static readonly LANDING_START = 2;
  static readonly LANDING_END = 3;

  /*
   * Slots of the anchor out values, the same as AnchorSlot in the wasm side.
   */
  static readonly ANCHOR_OFFSET = 0;
  static readonly ANCHOR_KEY_LENGTH = 1;
  static readonly ANCHOR_SLOTS = 2;

  /*
   * The smallest offset change that counts as a real move.
   */
  static get OFFSET_MOVED_THRESHOLD(): number {
    return offsetMovedThreshold;
  }

  /*
   * The diff of two key lists through the core's diffKeys, packed: the delete count and
   * previous indices, the insert count and next indices, the move count and from, to pairs.
   */
  static diffKeys(
    previous: ReadonlyArray<string>,
    next: ReadonlyArray<string>
  ): Int32Array {
    // Only the keys between the unchanged rows at both ends cross. They change nothing in the diff.
    const limit = Math.min(previous.length, next.length);
    let start = 0;
    while (start < limit && previous[start] === next[start]) ++start;
    let end = 0;
    while (
      end < limit - start &&
      previous[previous.length - 1 - end] === next[next.length - 1 - end]
    ) {
      ++end;
    }
    const previousTo = previous.length - end;
    const nextTo = next.length - end;
    const at = reserve(
      packedBytes(previous, start, previousTo) +
        packedBytes(next, start, nextTo)
    );
    const before = pack(previous, start, previousTo, at);
    const after = pack(next, start, nextTo, at + before.taken);
    const diff = takeInts(
      wasm()._ShadowListKitCore_nativeDiffKeys(
        before.packed,
        before.ends,
        previousTo - start,
        after.packed,
        after.ends,
        nextTo - start
      )
    );
    // Indices come back inside the middle. Every value but the three counts moves by start.
    const deletes = diff[0]!;
    const inserts = diff[1 + deletes]!;
    const counts = new Set([0, 1 + deletes, 2 + deletes + inserts]);
    for (let position = 0; position < diff.length; ++position) {
      if (!counts.has(position)) diff[position]! += start;
    }
    return diff;
  }

  /*
   * The core's insertionIndices and deletionIndices: sorted, each once, an insert past the
   * end at the end, a delete past the end dropped. Negative indices name no row.
   */
  static insertionIndices(
    indices: ReadonlyArray<number>,
    previousCount: number
  ): Int32Array {
    const at = reserve(indices.length * 4);
    putInts(at, indices);
    return takeInts(
      wasm()._ShadowListKitCore_nativeInsertionIndices(
        at,
        indices.length,
        previousCount
      )
    );
  }

  static deletionIndices(
    indices: ReadonlyArray<number>,
    previousCount: number
  ): Int32Array {
    const at = reserve(indices.length * 4);
    putInts(at, indices);
    return takeInts(
      wasm()._ShadowListKitCore_nativeDeletionIndices(
        at,
        indices.length,
        previousCount
      )
    );
  }

  /*
   * The wasm Peer, 0 once destroyed.
   */
  private handle: number;

  /*
   * The pass slots, see PASS_*. The view fills the inputs and reads the outputs.
   */
  readonly pass = new Float64Array(ShadowListKitCore.PASS_SLOTS);

  /*
   * Where the peer keeps its pass slots in the heap.
   */
  private readonly passSlots: number;

  constructor(
    private readonly measure: (row: number, crossSize: number) => number
  ) {
    this.handle = wasm()._ShadowListKitCore_nativeCreate();
    this.passSlots = wasm()._ShadowListKitCore_nativePassSlots(this.handle);
    cores.set(this.handle, new WeakRef(this));
    reclaimer.register(this, this.handle, this);
  }

  /*
   * Called by the driver during runPasses for every row in the measured range that has no size yet.
   */
  measureRow(row: number, crossSize: number): number {
    return this.measure(row, crossSize);
  }

  /*
   * Free the wasm peer. The core is unusable after this.
   */
  destroy(): void {
    if (this.handle === 0) return;
    wasm()._ShadowListKitCore_nativeDestroy(this.handle);
    cores.delete(this.handle);
    reclaimer.unregister(this);
    this.handle = 0;
  }

  setSettings(
    estimatedRowSize: number,
    overscan: number,
    startReachedThreshold: number,
    endReachedThreshold: number,
    numberOfColumns: number,
    inverted: boolean,
    followAppends: boolean,
    horizontal: boolean,
    snapToItem: boolean,
    snapAlignment: number
  ): void {
    wasm()._ShadowListKitCore_nativeSetSettings(
      this.handle,
      estimatedRowSize,
      overscan,
      startReachedThreshold,
      endReachedThreshold,
      numberOfColumns,
      inverted,
      followAppends,
      horizontal,
      snapToItem,
      snapAlignment
    );
  }

  setStickyIndices(indices: ReadonlyArray<number>): void {
    const at = reserve(indices.length * 4);
    putInts(at, indices);
    wasm()._ShadowListKitCore_nativeSetStickyIndices(
      this.handle,
      at,
      indices.length
    );
  }

  /*
   * Replace count keys from start with keys from until to. Keys cross as one UTF-8 buffer and
   * one array of where each key ends.
   */
  replaceKeys(
    start: number,
    count: number,
    keys: ReadonlyArray<string>,
    from: number,
    to: number
  ): void {
    const packed = pack(keys, from, to, reserve(packedBytes(keys, from, to)));
    wasm()._ShadowListKitCore_nativeReplaceKeys(
      this.handle,
      start,
      count,
      packed.packed,
      packed.ends,
      Math.max(0, to - from)
    );
  }

  insertKeys(
    indices: ReadonlyArray<number>,
    keys: ReadonlyArray<string>
  ): void {
    const count = Math.min(indices.length, keys.length);
    const at = reserve(align(count * 4) + packedBytes(keys, 0, count));
    putInts(at, indices.slice(0, count));
    const packed = pack(keys, 0, count, at + align(count * 4));
    wasm()._ShadowListKitCore_nativeInsertKeys(
      this.handle,
      at,
      packed.packed,
      packed.ends,
      count
    );
  }

  deleteKeys(indices: ReadonlyArray<number>): void {
    const at = reserve(indices.length * 4);
    putInts(at, indices);
    wasm()._ShadowListKitCore_nativeDeleteKeys(this.handle, at, indices.length);
  }

  markRemeasure(indices: ReadonlyArray<number>): void {
    const at = reserve(indices.length * 4);
    putInts(at, indices);
    wasm()._ShadowListKitCore_nativeMarkRemeasure(
      this.handle,
      at,
      indices.length
    );
  }

  /*
   * Run the core with the inputs in pass until every row of the measured range has a size and
   * corrections landed.
   * Measuring can grow the heap. The outputs are read from the heap as it is after the call.
   */
  runPasses(): void {
    const at = this.passSlots;
    wasm().HEAPF64.set(this.pass, at / 8);
    wasm()._ShadowListKitCore_nativeRunPasses(this.handle);
    const start = at / 8 + ShadowListKitCore.PASS_OUT_OFFSET;
    this.pass.set(
      wasm().HEAPF64.subarray(
        start,
        start + ShadowListKitCore.PASS_SLOTS - ShadowListKitCore.PASS_OUT_OFFSET
      ),
      ShadowListKitCore.PASS_OUT_OFFSET
    );
  }

  resetKeepingPosition(): void {
    wasm()._ShadowListKitCore_nativeResetKeepingPosition(this.handle);
  }

  get rowCount(): number {
    return wasm()._ShadowListKitCore_nativeRowCount(this.handle);
  }

  /*
   * Frames of rows low to high into out, four values each, x y width height.
   */
  copyRowRects(low: number, high: number, out: Float64Array): void {
    const count = Math.max(0, high - low + 1) * 4;
    const at = reserve(count * 8);
    wasm()._ShadowListKitCore_nativeCopyRowRects(this.handle, low, high, at);
    out.set(wasm().HEAPF64.subarray(at / 8, at / 8 + count));
  }

  rowRect(index: number, out: Float64Array): boolean {
    const at = reserve(4 * 8);
    if (wasm()._ShadowListKitCore_nativeRowRect(this.handle, index, at) === 0) {
      return false;
    }
    out.set(wasm().HEAPF64.subarray(at / 8, at / 8 + 4));
    return true;
  }

  footerStart(footerSize: number): number {
    return wasm()._ShadowListKitCore_nativeFooterStart(this.handle, footerSize);
  }

  /*
   * The rows overlapping the viewport as low and high, or null before the first layout.
   */
  visibleRange(): { low: number; high: number } | null {
    const at = reserve(2 * 4);
    if (wasm()._ShadowListKitCore_nativeVisibleRange(this.handle, at) === 0) {
      return null;
    }
    const heap = wasm().HEAP32;
    return { low: heap[at / 4]!, high: heap[at / 4 + 1]! };
  }

  indexOfKey(key: string): number {
    const at = reserve(key.length * 3);
    const length = putKey(key, at);
    return wasm()._ShadowListKitCore_nativeIndexOfKey(this.handle, at, length);
  }

  copyStickyFrames(out: Float64Array): boolean {
    const at = reserve(out.length * 8);
    if (
      wasm()._ShadowListKitCore_nativeCopyStickyFrames(
        this.handle,
        at,
        out.length
      ) === 0
    ) {
      return false;
    }
    out.set(wasm().HEAPF64.subarray(at / 8, at / 8 + out.length));
    return true;
  }

  scrollToRow(row: number, viewPosition: number): void {
    wasm()._ShadowListKitCore_nativeScrollToRow(this.handle, row, viewPosition);
  }

  scrollToStart(): void {
    wasm()._ShadowListKitCore_nativeScrollToStart(this.handle);
  }

  scrollToEnd(): void {
    wasm()._ShadowListKitCore_nativeScrollToEnd(this.handle);
  }

  nearestSnapOffset(target: number): number {
    return wasm()._ShadowListKitCore_nativeNearestSnapOffset(
      this.handle,
      target
    );
  }

  /*
   * Where an animated scroll to a row aims, or NaN for a row not placed yet.
   */
  animatedTargetOffset(
    index: number,
    viewPosition: number,
    windowAlong: number,
    maxOffset: number
  ): number {
    return wasm()._ShadowListKitCore_nativeAnimatedTargetOffset(
      this.handle,
      index,
      viewPosition,
      windowAlong,
      maxOffset
    );
  }

  setLanding(target: number, index = 0, viewPosition = 0): void {
    wasm()._ShadowListKitCore_nativeSetLanding(
      this.handle,
      target,
      index,
      viewPosition
    );
  }

  cancelLanding(): void {
    wasm()._ShadowListKitCore_nativeCancelLanding(this.handle);
  }

  /*
   * Send the landing's command after the animation ended. Returns whether there was one.
   */
  land(): boolean {
    return wasm()._ShadowListKitCore_nativeLand(this.handle) !== 0;
  }

  /*
   * Prefetch changes, packed: the prefetch count and rows, then the cancel count and rows.
   */
  updatePrefetch(low: number, high: number): Int32Array {
    return takeInts(
      wasm()._ShadowListKitCore_nativeUpdatePrefetch(this.handle, low, high)
    );
  }

  /*
   * The key of the row at the viewport start and the distance into it, or null.
   */
  anchorState(offset: number): { key: string; offset: number } | null {
    const at = reserve(ShadowListKitCore.ANCHOR_SLOTS * 8);
    const pointer = wasm()._ShadowListKitCore_nativeAnchorState(
      this.handle,
      offset,
      at
    );
    if (pointer === 0) return null;
    const heap = wasm().HEAPF64;
    const length = heap[at / 8 + ShadowListKitCore.ANCHOR_KEY_LENGTH]!;
    return {
      key: decoder.decode(wasm().HEAPU8.subarray(pointer, pointer + length)),
      offset: heap[at / 8 + ShadowListKitCore.ANCHOR_OFFSET]!,
    };
  }

  restoreAnchorState(key: string, offset: number): boolean {
    const at = reserve(key.length * 3);
    const length = putKey(key, at);
    return (
      wasm()._ShadowListKitCore_nativeRestoreAnchorState(
        this.handle,
        at,
        length,
        offset
      ) !== 0
    );
  }
}

/*
 * One version as the 32 bit halves of an int64, high then low, the type the core's
 * ContentVersions compares. A safe integer is itself. Another number is its float64 bits. A
 * string is two different 32 bit hashes of its UTF-16 units, FNV-1a and a shifted variant. A
 * change between two strings goes unseen only when both hashes collide.
 */
const versionBits = new Float64Array(1);
const versionWords = new Int32Array(versionBits.buffer);

/* eslint-disable no-bitwise -- the string hash works on 32 bit words. */
function versionHalves(
  version: number | string,
  out: Int32Array,
  at: number
): void {
  if (typeof version === 'string') {
    let high = 0x811c9dc5;
    let low = 0x050c5d1f;
    for (let index = 0; index < version.length; ++index) {
      const unit = version.charCodeAt(index);
      high = Math.imul(high ^ unit, 0x01000193);
      low = Math.imul(low ^ unit, 0x01000193) ^ (low >>> 15);
    }
    out[at] = high;
    out[at + 1] = low;
  } else if (Number.isSafeInteger(version)) {
    const high = Math.floor(version / 0x100000000);
    out[at] = high;
    out[at + 1] = version - high * 0x100000000;
  } else {
    versionBits[0] = version;
    out[at] = versionWords[1]!;
    out[at + 1] = versionWords[0]!;
  }
}
/* eslint-enable no-bitwise */

/*
 * What a list keeps across the cores it makes and drops: the batch performBatchUpdates collects
 * and the content versions applyChanges compares, the core's BatchCollector and ContentVersions
 * in C++. The list drops its core on disconnect, and this lives as long as the list. The peer is
 * made on first use, which needs the module loaded. Versions recorded before that wait here.
 */
export class ShadowListKitListState {
  private peer = 0;
  private pendingRecord: {
    keys: ReadonlyArray<string>;
    versions: ReadonlyArray<number | string>;
  } | null = null;

  private get handle(): number {
    if (this.peer === 0) {
      this.peer = wasm()._ShadowListKitListState_nativeCreate();
      stateReclaimer.register(this, this.peer);
    }
    return this.peer;
  }

  /*
   * performBatchUpdates' block starts and returns. endBatch returns whether it was the
   * outermost, after which the batch is taken.
   */
  beginBatch(): void {
    wasm()._ShadowListKitListState_nativeBeginBatch(this.handle);
  }

  endBatch(): boolean {
    return wasm()._ShadowListKitListState_nativeEndBatch(this.handle) !== 0;
  }

  get isBatching(): boolean {
    return (
      this.peer !== 0 &&
      wasm()._ShadowListKitListState_nativeIsBatching(this.peer) !== 0
    );
  }

  batchDeleted(indices: ReadonlyArray<number>): void {
    const handle = this.handle;
    const at = reserve(indices.length * 4);
    putInts(at, indices);
    wasm()._ShadowListKitListState_nativeBatchDeleted(
      handle,
      at,
      indices.length
    );
  }

  batchInserted(indices: ReadonlyArray<number>): void {
    const handle = this.handle;
    const at = reserve(indices.length * 4);
    putInts(at, indices);
    wasm()._ShadowListKitListState_nativeBatchInserted(
      handle,
      at,
      indices.length
    );
  }

  batchMoved(from: number, to: number): void {
    wasm()._ShadowListKitListState_nativeBatchMoved(this.handle, from, to);
  }

  batchReloaded(indices: ReadonlyArray<number>): void {
    const handle = this.handle;
    const at = reserve(indices.length * 4);
    putInts(at, indices);
    wasm()._ShadowListKitListState_nativeBatchReloaded(
      handle,
      at,
      indices.length
    );
  }

  batchNeedsReload(): void {
    wasm()._ShadowListKitListState_nativeBatchNeedsReload(this.handle);
  }

  /*
   * Take the collected batch: null when it is empty, otherwise whether it needs a reload (1 or
   * 0) then the reloaded items. planTakenBatch plans it once.
   */
  takeBatch(): Int32Array | null {
    const pointer = wasm()._ShadowListKitListState_nativeTakeBatch(this.handle);
    return pointer === 0 ? null : takeInts(pointer);
  }

  /*
   * The source of every row after the taken batch, a previous index or -1 for an insert, or
   * null when the batch does not add up. See the core's planBatch.
   */
  planTakenBatch(previousCount: number, nextCount: number): Int32Array | null {
    const pointer = wasm()._ShadowListKitListState_nativePlanTakenBatch(
      this.handle,
      previousCount,
      nextCount
    );
    return pointer === 0 ? null : takeInts(pointer);
  }

  /*
   * Forget every version and keep these, one per item key.
   */
  recordContentVersions(
    keys: ReadonlyArray<string>,
    versions: ReadonlyArray<number | string>
  ): void {
    if (!isShadowListKitLoaded()) {
      this.pendingRecord = { keys, versions };
      return;
    }
    this.pendingRecord = null;
    const handle = this.handle;
    const { at, ends, packed } = this.packVersions(keys, versions);
    wasm()._ShadowListKitListState_nativeRecordContentVersions(
      handle,
      packed,
      ends,
      keys.length,
      at
    );
  }

  /*
   * The items whose version changed under the same key, low to high. The new versions are kept.
   */
  updateContentVersions(
    keys: ReadonlyArray<string>,
    versions: ReadonlyArray<number | string>
  ): Int32Array {
    if (this.pendingRecord) {
      this.recordContentVersions(
        this.pendingRecord.keys,
        this.pendingRecord.versions
      );
    }
    const handle = this.handle;
    const { at, ends, packed } = this.packVersions(keys, versions);
    return takeInts(
      wasm()._ShadowListKitListState_nativeUpdateContentVersions(
        handle,
        packed,
        ends,
        keys.length,
        at
      )
    );
  }

  /*
   * The versions as halves, then the keys packed after them.
   */
  private packVersions(
    keys: ReadonlyArray<string>,
    versions: ReadonlyArray<number | string>
  ): { at: number; ends: number; packed: number } {
    const halves = align(keys.length * 8);
    const at = reserve(halves + packedBytes(keys, 0, keys.length));
    const heap = wasm().HEAP32;
    for (let item = 0; item < keys.length; ++item) {
      versionHalves(versions[item] ?? 0, heap, at / 4 + item * 2);
    }
    const { ends, packed } = pack(keys, 0, keys.length, at + halves);
    return { at, ends, packed };
  }
}
