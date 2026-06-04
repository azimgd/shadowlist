/*
 * Hand-written type surface for the Emscripten/embind module produced by
 * scripts/build-wasm.sh. Mirrors the bindings declared in cpp/Binding.cpp.
 */

export interface ElementLayout {
  index: number;
  key: string;
  offsetX: number;
  offsetY: number;
  width: number;
  height: number;
  measured: boolean;
}

export interface VisibleIndices {
  visibleStartIndex: number;
  visibleEndIndex: number;
}

export interface ViewableIndices {
  viewableStartIndex: number;
  viewableEndIndex: number;
}

export interface StateUpdate {
  changed: boolean;
  applyContainerOffset: boolean;
  containerOffsetX: number;
  containerOffsetY: number;
  totalContainerWidth: number;
  totalContainerHeight: number;
}

/*
 * Embind-bound handle. Must be released with delete() to free the C++ instance.
 */
export interface ShadowlistCoreInstance {
  update(
    keys: string[],
    containerOffsetX: number,
    containerOffsetY: number,
    windowContainerWidth: number,
    windowContainerHeight: number,
    headerSize: number,
    footerSize: number,
    inverted: boolean,
    horizontal: boolean,
    columns: number,
    estimatedWidth: number,
    estimatedHeight: number,
    userScrolled: boolean,
    stickyHeader: boolean,
    stickyFooter: boolean,
    startReachedThreshold: number,
    endReachedThreshold: number,
    viewablePercentThreshold: number
  ): void;
  updateElementAtIndex(index: number, width: number, height: number): void;
  recomputeTotalSize(): void;
  getElementsSize(): number;
  getElementAtIndex(index: number): ElementLayout;
  getVisibleIndices(): VisibleIndices;
  getViewableIndices(): ViewableIndices;
  resolveStateUpdate(
    prevContainerOffsetX: number,
    prevContainerOffsetY: number,
    prevTotalContainerWidth: number,
    prevTotalContainerHeight: number
  ): StateUpdate;
  getFooterOffset(footerSize: number): number;
  getStickyHeaderOffset(): number;
  getStickyFooterOffset(footerSize: number): number;
  scrollToIndex(index: number): void;
  requestScrollToIndex(commandIndex: number, commandNonce: number, propIndex: number): void;
  toggleEndReached(enabled: boolean): void;
  toggleStartReached(enabled: boolean): void;
  setOnEndReached(callback: () => void): void;
  setOnStartReached(callback: () => void): void;
  setOnVisibleIndicesChange(callback: (startIndex: number, endIndex: number) => void): void;
  setOnViewableIndicesChange(callback: (startIndex: number, endIndex: number) => void): void;
  setOnScroll(callback: (containerOffsetX: number, containerOffsetY: number) => void): void;
  delete(): void;
}

export interface ShadowlistCoreModule {
  ShadowlistCore: new () => ShadowlistCoreInstance;
}

export interface ShadowlistCoreModuleOptions {
  wasmBinary?: ArrayBuffer | Uint8Array;
  locateFile?: (path: string, scriptDirectory: string) => string;
  [key: string]: unknown;
}

declare const createShadowlistCoreModule: (
  options?: ShadowlistCoreModuleOptions
) => Promise<ShadowlistCoreModule>;

export default createShadowlistCoreModule;
