import { useEffect, useState } from 'react';
import { StyleSheet, processColor } from 'react-native';
import type { ShadowListNativeBind } from '../types';

/*
 * The JSI object ShadowListNativeJSI.cpp installs. Every call is synchronous and mutates the
 * list's native store; mutations also schedule a commit of the list.
 */
export interface ShadowListNativeBinding {
  setData(
    listId: string,
    items: ReadonlyArray<unknown>,
    keys: string[],
    templates: string[] | null
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
  // After the mutations made so far are laid out; -1 is the end, -2 the start (offset 0).
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
  // Keeps the list's engine alive while the returned handle is held (or until close).
  open(listId: string): ShadowListNativeHandle;
  close(handle: ShadowListNativeHandle): void;
}

// Opaque; a JSI host object holding the engine.
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
 * The binding is installed on the JS thread shortly after the renderer starts. A list rendered
 * before that waits a frame or two rather than mounting without its data.
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
 * Per runtime (and per evaluation of this module): the counter restarts after a JS reload or a
 * Fast Refresh of this file, while engines from before can still be alive for a moment.
 */
const RUNTIME_NONCE = Math.floor(Math.random() * 0x7fffffff).toString(36);

export function createShadowListNativeId(): string {
  nextListId += 1;
  return `shadowlist-native-${RUNTIME_NONCE}-${nextListId}`;
}

/*
 * Template element metadata rides on `nativeID`, the one string prop every host component
 * forwards to native. The engine reads it when it compiles the template and strips it from the
 * rows it builds (keeping it only on elements with an action, which must not be flattened away
 * or their touches would land on an ancestor).
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

export function encodeTemplateMarker(name: string): string {
  return TEMPLATE_MARKER + name;
}

function isColorProp(prop: string): boolean {
  return prop === 'color' || (prop.length > 5 && prop.endsWith('Color'));
}

/*
 * A style for setTemplateStyle, in the form Fabric parses raw props: flattened, with colors
 * processed to numbers the way React Native's own prop diffing sends them.
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
