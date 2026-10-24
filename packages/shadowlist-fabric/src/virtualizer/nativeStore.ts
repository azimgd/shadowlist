import { useRef } from 'react';
import { TurboModuleRegistry, type TurboModule } from 'react-native';
import type { ItemSizeSpec } from '../types';

/*
 * The native key store a list writes its row keys and row size specs into. They never go
 * through props. See ShadowListStoreJsi.h for what each function does. Every write names a new
 * version, the props name the version a render wrote, and a write against a base the store has
 * dropped fails, after which the caller writes everything.
 */
export interface NativeStoreHandle {
  readonly id: number;
}

export interface ShadowListStoreModule extends TurboModule {
  createStore(): NativeStoreHandle;
  setKeys(
    store: NativeStoreHandle,
    version: number,
    keys: ReadonlyArray<string>
  ): boolean;
  spliceKeys(
    store: NativeStoreHandle,
    version: number,
    baseVersion: number,
    start: number,
    deleteCount: number,
    inserted: ReadonlyArray<string>
  ): boolean;
  setSections(
    store: NativeStoreHandle,
    version: number,
    baseVersion: number,
    flags: number,
    sectionKeys: ReadonlyArray<string>,
    itemCounts: ReadonlyArray<number>,
    reuse: ReadonlyArray<number>,
    itemKeys: ReadonlyArray<string>
  ): boolean;
  resetTree(
    store: NativeStoreHandle,
    version: number,
    keys: ReadonlyArray<string>,
    depths: ReadonlyArray<number>,
    flags: ReadonlyArray<number>
  ): boolean;
  expandTree(
    store: NativeStoreHandle,
    version: number,
    baseVersion: number,
    row: number,
    keys: ReadonlyArray<string>,
    depths: ReadonlyArray<number>,
    flags: ReadonlyArray<number>
  ): number;
  collapseTree(
    store: NativeStoreHandle,
    version: number,
    baseVersion: number,
    row: number
  ): number;
  indexOfKey(
    store: NativeStoreHandle,
    version: number,
    key: string,
    hint: number
  ): number;
  setSizeSpecs(
    store: NativeStoreHandle,
    version: number,
    keys: ReadonlyArray<string>,
    specs: ReadonlyArray<ItemSizeSpec>
  ): void;
}

/*
 * setSections flags.
 */
export const SECTIONS_HEADER = 1;
export const SECTIONS_FOOTER = 2;
export const SECTIONS_STICKY = 4;
export const SECTIONS_QUALIFIED = 8;

/*
 * Tree row flags, the same for resetTree and expandTree.
 */
export const TREE_HAS_CHILDREN = 1;
export const TREE_EXPANDED = 2;

/*
 * indexOfKey's answer for a version the store no longer has.
 */
export const VERSION_GONE = -2;

let storeModule: ShadowListStoreModule | null | undefined;

/*
 * The module, or null where it is missing, like in tests or an app built without it. Lists then
 * send their keys in props. Looked up on first use, after the native side registered it.
 */
export function shadowListStoreModule(): ShadowListStoreModule | null {
  if (storeModule === undefined) {
    try {
      storeModule =
        TurboModuleRegistry.get<ShadowListStoreModule>('ShadowListStore') ??
        null;
    } catch {
      storeModule = null;
    }
  }
  return storeModule;
}

/*
 * Tests swap the module. undefined looks it up again.
 */
export function setShadowListStoreModule(
  module: ShadowListStoreModule | null | undefined
): void {
  storeModule = module;
}

/*
 * One list's store, and the last version written to it. Versions only grow.
 */
export interface ListStore {
  module: ShadowListStoreModule;
  handle: NativeStoreHandle;
  id: number;
  version: number;
}

export function nextVersion(store: ListStore): number {
  store.version += 1;
  return store.version;
}

/*
 * The list's store, made on its first render, or null without the module. The store lives as
 * long as the list, and native keeps it alive while its views use it.
 */
export function useListStore(): ListStore | null {
  const storeRef = useRef<ListStore | null | undefined>(undefined);
  if (storeRef.current === undefined) {
    const module = shadowListStoreModule();
    if (module) {
      const handle = module.createStore();
      storeRef.current = { module, handle, id: handle.id, version: 0 };
    } else {
      storeRef.current = null;
    }
  }
  return storeRef.current;
}
