import createShadowListUtilsModule, {
  type ShadowListUtilsModule,
} from '../shadowlistutils.mjs';
import {
  ShadowListWasmLoader,
  type ShadowListWasmLoadOptions,
} from '../ShadowListWasmLoader.js';
import type { PollTally } from './PollTally.js';
import type { RelativeTime } from './RelativeTime.js';

export interface ShadowListUtilsLoadOptions extends ShadowListWasmLoadOptions {}

/*
 * Result slots mirrored by ShadowListUtilsCoreWasm.cpp.
 */
const TIME_UNIT = 0;
const TIME_COUNT = 1;
const TIME_LENGTH = 2;
const POLL_TOTAL_VOTES = 0;
const POLL_LEADING_INDEX = 1;
const POLL_LENGTH = 2;

let wasmModule: ShadowListUtilsModule | null = null;
let scratch = 0;
let scratchBytes = 0;
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const loader = new ShadowListWasmLoader(
  createShadowListUtilsModule,
  (loaded) => {
    wasmModule = loaded;
  }
);

/*
 * Loads the separate utility binary once. List-only applications never load this module.
 * Failed loads can be retried with bytes, a compiled module, or another locateFile.
 */
export function loadShadowListUtils(
  options?: ShadowListUtilsLoadOptions
): Promise<void> {
  return loader.load(options);
}

export function isShadowListUtilsLoaded(): boolean {
  return wasmModule !== null;
}

function wasm(bytes: number): ShadowListUtilsModule {
  if (!wasmModule)
    throw new Error('Call loadShadowListUtils() before using utilities');
  if (bytes > scratchBytes) {
    if (scratch) wasmModule._free(scratch);
    scratchBytes = Math.max(bytes, scratchBytes * 2, 32);
    scratch = wasmModule._malloc(scratchBytes);
  }
  return wasmModule;
}

export function avatarColorIndex(name: string, paletteSize: number): number {
  const bytes = encoder.encode(name);
  const core = wasm(bytes.length + 8);
  core.HEAPU8.set(bytes, scratch);
  return core._ShadowListUtilsCore_nativeAvatarColorIndex(
    scratch,
    bytes.length,
    paletteSize
  );
}

/*
 * Raw first and last initials. The host applies its locale's uppercase policy.
 */
export function avatarInitials(name: string): string {
  const bytes = encoder.encode(name);
  const result = Math.ceil(bytes.length / 8) * 8;
  const core = wasm(result + 8);
  core.HEAPU8.set(bytes, scratch);
  const count = core._ShadowListUtilsCore_nativeAvatarInitials(
    scratch,
    bytes.length,
    scratch + result
  );
  return decoder.decode(
    core.HEAPU8.subarray(scratch + result, scratch + result + count)
  );
}

export function relativeTime(dateMs: number, nowMs: number): RelativeTime {
  const core = wasm(TIME_LENGTH * 8);
  core._ShadowListUtilsCore_nativeRelativeTime(dateMs, nowMs, scratch);
  const units = ['now', 'minutes', 'hours', 'days', 'date'] as const;
  return {
    unit: units[core.HEAPF64[scratch / 8 + TIME_UNIT]!]!,
    count: core.HEAPF64[scratch / 8 + TIME_COUNT]!,
  };
}

export function relativeTimeHasDate(dateMs: number): boolean {
  return Boolean(
    wasm(0)._ShadowListUtilsCore_nativeRelativeTimeHasDate(dateMs)
  );
}

export function pollTally(votes: ReadonlyArray<number>): PollTally {
  const core = wasm((votes.length + POLL_LENGTH) * 8);
  core.HEAPF64.set(votes, scratch / 8);
  const result = scratch + votes.length * 8;
  core._ShadowListUtilsCore_nativePollTally(scratch, votes.length, result);
  return {
    totalVotes: core.HEAPF64[result / 8 + POLL_TOTAL_VOTES]!,
    leadingIndex: core.HEAPF64[result / 8 + POLL_LEADING_INDEX]!,
  };
}

export function pollPercent(votes: number, totalVotes: number): number {
  return wasm(0)._ShadowListUtilsCore_nativePollPercent(votes, totalVotes);
}
