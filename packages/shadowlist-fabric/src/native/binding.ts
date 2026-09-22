import { useEffect, useState } from 'react';
import { StyleSheet, processColor } from 'react-native';
import type { ShadowListNativeBind } from '../types';

/*
 * The object ShadowListNativeJSI.cpp installs. Every call runs synchronously on the list's
 * native store, and changes also schedule a commit of the list.
 */
export interface ShadowListNativeBinding {
  /*
   * scrollToStart scrolls to the top in the same commit as the new rows.
   */
  setData(
    listId: string,
    items: ReadonlyArray<unknown>,
    keys: string[],
    templates: string[] | null,
    scrollToStart?: boolean
  ): number;
  /*
   * Rows by position. See ShadowListNativeIndexedData.
   */
  setIndexed(
    listId: string,
    count: number,
    order: Int32Array | undefined,
    indexField: string,
    valueField: string,
    extraIndices: number[],
    extraItems: ReadonlyArray<unknown>,
    extraTemplates: string[] | null,
    scrollToStart?: boolean,
    ids?: Int32Array
  ): number;
  insertItems(
    listId: string,
    index: number,
    items: ReadonlyArray<unknown>,
    keys: string[],
    templates: string[] | null
  ): number;
  updateItem(
    listId: string,
    key: string,
    patch: unknown,
    template: string | null,
    replace: boolean
  ): boolean;
  removeItems(listId: string, keys: ReadonlyArray<string>): number;
  moveItem(listId: string, key: string, toIndex: number): boolean;
  // Runs after earlier changes are laid out. Index -1 means the end and -2 the top.
  scrollToIndex(listId: string, index: number, viewPosition: number): void;
  setTemplateStyle(
    listId: string,
    template: string,
    elementId: string,
    style: Record<string, unknown> | null
  ): void;
  configure(listId: string, config: ShadowListNativeConfig): void;
  getItem(listId: string, key: string): unknown;
  getKeys(listId: string): string[];
  getCount(listId: string): number;
  resolveTag(
    listId: string,
    tag: number
  ): { key: string; index: number; repeatIndex: number } | null;
  // Keeps the engine alive while the returned handle is held, or until close.
  open(listId: string): ShadowListNativeHandle;
  close(handle: ShadowListNativeHandle): void;
}

/*
 * Opaque native object holding the engine.
 */
export type ShadowListNativeHandle = object;

export interface ShadowListNativeConfig {
  initialRows?: number;
  padRows?: number;
  cacheRows?: number;
}

interface BindingGlobal {
  __shadowListNative?: ShadowListNativeBinding;
}

export function getShadowListNativeBinding():
  | ShadowListNativeBinding
  | undefined {
  return (globalThis as BindingGlobal).__shadowListNative;
}

/*
 * The binding shows up on the JS thread shortly after the renderer starts. A list rendered
 * before that waits a frame or two instead of mounting without data.
 */
export function useShadowListNativeBinding():
  | ShadowListNativeBinding
  | undefined {
  const [binding, setBinding] = useState(getShadowListNativeBinding);
  useEffect(() => {
    if (binding) return;
    let frame = 0;
    const poll = () => {
      const installed = getShadowListNativeBinding();
      if (installed) {
        setBinding(() => installed);
      } else {
        frame = requestAnimationFrame(poll);
      }
    };
    frame = requestAnimationFrame(poll);
    return () => cancelAnimationFrame(frame);
  }, [binding]);
  return binding;
}

let nextListId = 0;

/*
 * The counter restarts after a JS reload or a Fast Refresh of this file, while old engines
 * can still be alive for a moment.
 */
const RUNTIME_NONCE = Math.floor(Math.random() * 0x7fffffff).toString(36);

export function createShadowListNativeId(): string {
  nextListId += 1;
  return `shadowlist-native-${RUNTIME_NONCE}-${nextListId}`;
}

/*
 * Template metadata travels in nativeID, the one string prop every host component sends to
 * native. The engine reads it when it compiles the template and removes it from built rows.
 * Elements with an action keep it, so they aren't flattened away and their touches don't
 * land on a parent.
 */
export const ELEMENT_MARKER = 'shadowlist:';
export const TEMPLATE_MARKER = 'shadowlist-template:';

export interface ElementMarker {
  id?: string;
  bind?: ShadowListNativeBind;
  action?: string;
  repeat?: string;
  repeatMax?: number;
}

export function encodeElementMarker({
  id,
  bind,
  action,
  repeat,
  repeatMax,
}: ElementMarker): string | undefined {
  const spec: {
    i?: string;
    b?: ShadowListNativeBind;
    a?: 1;
    r?: string;
    m?: number;
  } = {};
  if (id) spec.i = id;
  if (bind && Object.keys(bind).length > 0) spec.b = bind;
  if (action) spec.a = 1;
  if (repeat !== undefined) {
    spec.r = repeat;
    if (repeatMax !== undefined && repeatMax >= 0) spec.m = repeatMax;
  }
  if (
    spec.i === undefined &&
    spec.b === undefined &&
    spec.a === undefined &&
    spec.r === undefined
  ) {
    return undefined;
  }
  return ELEMENT_MARKER + JSON.stringify(spec);
}

/*
 * Template metadata for any host component, like your own native view in a template:
 * <MyNativeView {...shadowListNativeProps({ bind: { row: 'row' } })} />
 * The component parses bound props itself, like a style prop on a View. For presses, wrap
 * it in a ShadowListNative.View with an action, since this adds no touch handling.
 */
export function shadowListNativeProps({
  id,
  bind,
}: {
  id?: string;
  bind?: ShadowListNativeBind;
}): { nativeID?: string } {
  const nativeID = encodeElementMarker({ id, bind });
  return nativeID === undefined ? {} : { nativeID };
}

export function encodeTemplateMarker(name: string): string {
  return TEMPLATE_MARKER + name;
}

function isColorProp(prop: string): boolean {
  return prop === 'color' || (prop.length > 5 && prop.endsWith('Color'));
}

/*
 * A style for setTemplateStyle in the shape Fabric expects. It's flattened, and colors are
 * turned into numbers the way React Native sends them.
 */
export function toNativeStyle(style: unknown): Record<string, unknown> | null {
  const flat = StyleSheet.flatten(style as never) as
    | Record<string, unknown>
    | undefined;
  if (!flat) return null;
  const result: Record<string, unknown> = {};
  for (const [prop, value] of Object.entries(flat)) {
    if (value === undefined) continue;
    if (isColorProp(prop) && value !== null) {
      const color = processColor(value as never);
      result[prop] = typeof color === 'number' ? color : null;
    } else {
      result[prop] = value;
    }
  }
  return result;
}
