import type { ShadowListWasmModuleOptions } from './ShadowListWasmLoader.js';

export interface ShadowListUtilsModule {
  HEAPU8: Uint8Array;
  HEAPF64: Float64Array;
  _malloc(bytes: number): number;
  _free(pointer: number): void;
  _ShadowListUtilsCore_nativeAvatarColorIndex(
    name: number,
    length: number,
    paletteSize: number
  ): number;
  _ShadowListUtilsCore_nativeAvatarInitials(
    name: number,
    length: number,
    result: number
  ): number;
  _ShadowListUtilsCore_nativeRelativeTime(
    dateMs: number,
    nowMs: number,
    result: number
  ): void;
  _ShadowListUtilsCore_nativeRelativeTimeHasDate(dateMs: number): boolean;
  _ShadowListUtilsCore_nativePollTally(
    votes: number,
    count: number,
    result: number
  ): void;
  _ShadowListUtilsCore_nativePollPercent(
    votes: number,
    totalVotes: number
  ): number;
}

export default function createShadowListUtilsModule(
  options?: ShadowListWasmModuleOptions
): Promise<ShadowListUtilsModule>;
