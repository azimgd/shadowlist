/*
 * The Emscripten module build.sh compiles from cpp/ShadowListKitCoreWasm.cpp. Pointers and handles are
 * byte offsets into the heap. A C++ bool comes back as 0 or 1.
 */
export interface ShadowListKitModule {
  HEAP32: Int32Array;
  HEAPF64: Float64Array;
  HEAPU8: Uint8Array;
  shadowListKitMeasureRow?: (
    handle: number,
    row: number,
    crossSize: number
  ) => number;
  _malloc(bytes: number): number;
  _free(pointer: number): void;
  _ShadowListKitCore_nativeCreate(): number;
  _ShadowListKitCore_nativeDestroy(handle: number): void;
  _ShadowListKitCore_nativeSetSettings(
    handle: number,
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
  ): void;
  _ShadowListKitCore_nativeSetStickyIndices(
    handle: number,
    indices: number,
    count: number
  ): void;
  _ShadowListKitCore_nativeReplaceKeys(
    handle: number,
    start: number,
    count: number,
    packed: number,
    ends: number,
    keyCount: number
  ): void;
  _ShadowListKitCore_nativeInsertKeys(
    handle: number,
    indices: number,
    packed: number,
    ends: number,
    keyCount: number
  ): void;
  _ShadowListKitCore_nativeDeleteKeys(
    handle: number,
    indices: number,
    count: number
  ): void;
  _ShadowListKitCore_nativeMarkRemeasure(
    handle: number,
    indices: number,
    count: number
  ): void;
  _ShadowListKitCore_nativePassSlots(handle: number): number;
  _ShadowListKitCore_nativeRunPasses(handle: number): void;
  _ShadowListKitCore_nativeResetKeepingPosition(handle: number): void;
  _ShadowListKitCore_nativeRowCount(handle: number): number;
  _ShadowListKitCore_nativeCopyRowRects(
    handle: number,
    low: number,
    high: number,
    out: number
  ): void;
  _ShadowListKitCore_nativeRowRect(
    handle: number,
    index: number,
    out: number
  ): number;
  _ShadowListKitCore_nativeFooterStart(
    handle: number,
    footerSize: number
  ): number;
  _ShadowListKitCore_nativeOffsetMovedThreshold(): number;
  _ShadowListKitCore_nativeVisibleRange(handle: number, out: number): number;
  _ShadowListKitCore_nativeIndexOfKey(
    handle: number,
    key: number,
    length: number
  ): number;
  _ShadowListKitCore_nativeCopyStickyFrames(
    handle: number,
    out: number,
    capacity: number
  ): number;
  _ShadowListKitCore_nativeScrollToRow(
    handle: number,
    row: number,
    viewPosition: number
  ): void;
  _ShadowListKitCore_nativeScrollToStart(handle: number): void;
  _ShadowListKitCore_nativeScrollToEnd(handle: number): void;
  _ShadowListKitCore_nativeNearestSnapOffset(
    handle: number,
    target: number
  ): number;
  _ShadowListKitCore_nativeAnimatedTargetOffset(
    handle: number,
    index: number,
    viewPosition: number,
    windowAlong: number,
    maxOffset: number
  ): number;
  _ShadowListKitCore_nativeSetLanding(
    handle: number,
    target: number,
    index: number,
    viewPosition: number
  ): void;
  _ShadowListKitCore_nativeCancelLanding(handle: number): void;
  _ShadowListKitCore_nativeLand(handle: number): number;
  _ShadowListKitCore_nativeUpdatePrefetch(
    handle: number,
    low: number,
    high: number
  ): number;
  _ShadowListKitCore_nativeAnchorState(
    handle: number,
    offset: number,
    out: number
  ): number;
  _ShadowListKitCore_nativeRestoreAnchorState(
    handle: number,
    key: number,
    length: number,
    offset: number
  ): number;
  _ShadowListKitCore_nativeDiffKeys(
    previousPacked: number,
    previousEnds: number,
    previousCount: number,
    nextPacked: number,
    nextEnds: number,
    nextCount: number
  ): number;
  _ShadowListKitCore_nativeInsertionIndices(
    indices: number,
    count: number,
    previousCount: number
  ): number;
  _ShadowListKitCore_nativeDeletionIndices(
    indices: number,
    count: number,
    previousCount: number
  ): number;
  _ShadowListKitListState_nativeCreate(): number;
  _ShadowListKitListState_nativeDestroy(handle: number): void;
  _ShadowListKitListState_nativeBeginBatch(handle: number): void;
  _ShadowListKitListState_nativeEndBatch(handle: number): number;
  _ShadowListKitListState_nativeIsBatching(handle: number): number;
  _ShadowListKitListState_nativeBatchDeleted(
    handle: number,
    indices: number,
    count: number
  ): void;
  _ShadowListKitListState_nativeBatchInserted(
    handle: number,
    indices: number,
    count: number
  ): void;
  _ShadowListKitListState_nativeBatchMoved(
    handle: number,
    from: number,
    to: number
  ): void;
  _ShadowListKitListState_nativeBatchReloaded(
    handle: number,
    indices: number,
    count: number
  ): void;
  _ShadowListKitListState_nativeBatchNeedsReload(handle: number): void;
  _ShadowListKitListState_nativeTakeBatch(handle: number): number;
  _ShadowListKitListState_nativePlanTakenBatch(
    handle: number,
    previousCount: number,
    nextCount: number
  ): number;
  _ShadowListKitListState_nativeRecordContentVersions(
    handle: number,
    packed: number,
    ends: number,
    count: number,
    versions: number
  ): void;
  _ShadowListKitListState_nativeUpdateContentVersions(
    handle: number,
    packed: number,
    ends: number,
    count: number,
    versions: number
  ): number;
}

export default function createShadowListKitModule(options?: {
  locateFile?: (path: string, scriptDirectory: string) => string;
}): Promise<ShadowListKitModule>;
