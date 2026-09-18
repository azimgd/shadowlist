import { useRef } from 'react';

function shallowEqual<T extends object>(a: T, b: T): boolean {
  const aKeys = Object.keys(a) as (keyof T)[];
  if (aKeys.length !== Object.keys(b).length) {
    return false;
  }
  return aKeys.every((key) => Object.is(a[key], b[key]));
}

function mergeLabels<T extends object>(defaults: T, overrides?: Partial<T>): T {
  if (overrides === undefined) {
    return defaults;
  }
  const labels = { ...defaults };
  for (const key of Object.keys(overrides) as (keyof T)[]) {
    const value = overrides[key];
    if (value !== undefined) {
      labels[key] = value as T[keyof T];
    }
  }
  return labels;
}

/*
 * Returns the same object while the merged values are unchanged, so an inline `labels={{...}}`
 * prop does not invalidate memoized rows on every parent render.
 */
export function useLabels<T extends object>(
  defaults: T,
  overrides?: Partial<T>
): T {
  const cached = useRef<T | undefined>(undefined);
  const next = mergeLabels(defaults, overrides);
  if (cached.current !== undefined && shallowEqual(cached.current, next)) {
    return cached.current;
  }
  cached.current = next;
  return next;
}
