import { isValidElement, type ComponentType, type ReactElement } from 'react';

/*
 * What a row's own separator shows beyond its items: whether it is highlighted and the props
 * a row gave it through updateProps. A row's separator sits below it, between it and the next
 * row.
 */
export interface SeparatorState {
  highlighted: boolean;
  props: Readonly<Record<string, unknown>> | null;
}

const RESTING: SeparatorState = { highlighted: false, props: null };

/*
 * Separator state by row key, read by each row through useSyncExternalStore. Only rows whose
 * state changed render again. The store stays empty until a row highlights.
 */
export class SeparatorStore {
  private states = new Map<string, SeparatorState>();
  private listeners = new Map<string, Set<() => void>>();

  get(key: string): SeparatorState {
    return this.states.get(key) ?? RESTING;
  }

  subscribe(key: string, listener: () => void): () => void {
    let set = this.listeners.get(key);
    if (!set) {
      set = new Set();
      this.listeners.set(key, set);
    }
    set.add(listener);
    return () => {
      set!.delete(listener);
      if (set!.size === 0) this.listeners.delete(key);
    };
  }

  setHighlighted(key: string, highlighted: boolean): void {
    const state = this.get(key);
    if (state.highlighted === highlighted) return;
    this.write(key, { ...state, highlighted });
  }

  updateProps(key: string, props: Readonly<Record<string, unknown>>): void {
    const state = this.get(key);
    this.write(key, { ...state, props: { ...state.props, ...props } });
  }

  private write(key: string, state: SeparatorState): void {
    if (!state.highlighted && state.props === null) {
      this.states.delete(key);
    } else {
      this.states.set(key, state);
    }
    this.listeners.get(key)?.forEach((listener) => listener());
  }
}

/*
 * A separator is a component when it takes props. A React element, or a function
 * without parameters, renders once and every row shares it.
 */
export function separatorComponentOf<PropsT>(
  separator: ReactElement | ComponentType<PropsT> | null | undefined
): ComponentType<PropsT> | null {
  if (!separator || isValidElement(separator)) return null;
  if (typeof separator === 'function' && separator.length === 0) return null;
  return separator as ComponentType<PropsT>;
}

export function sharedSeparatorOf(
  separator: unknown
): ReactElement | (() => ReactElement | null) | null {
  if (!separator) return null;
  if (isValidElement(separator)) return separator;
  if (typeof separator === 'function' && separator.length === 0) {
    return separator as () => ReactElement | null;
  }
  return null;
}
