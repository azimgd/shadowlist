import { useRef, type ReactElement } from 'react';

function shallowEqualProps(a: unknown, b: unknown): boolean {
  if (Object.is(a, b)) return true;
  if (
    typeof a !== 'object' ||
    a === null ||
    typeof b !== 'object' ||
    b === null
  ) {
    return false;
  }
  const aKeys = Object.keys(a);
  if (aKeys.length !== Object.keys(b).length) return false;
  return aKeys.every(
    (key) =>
      Object.prototype.hasOwnProperty.call(b, key) &&
      Object.is(
        (a as Record<string, unknown>)[key],
        (b as Record<string, unknown>)[key]
      )
  );
}

/*
 * Keeps the previous React element while the new one is the same component, key and props.
 *
 * Callers pass React elements like the separator inline, which makes a new one every render. It
 * sits inside every row. Each new one rebuilds all mounted rows. On a grouped list this
 * is the biggest source of row re-renders. The list handles it here.
 */
export function useStableReactElement(
  reactElement: ReactElement | null
): ReactElement | null {
  const previousRef = useRef<ReactElement | null>(null);
  const previous = previousRef.current;

  if (reactElement !== null && previous !== null && reactElement !== previous) {
    if (
      reactElement.type === previous.type &&
      reactElement.key === previous.key &&
      shallowEqualProps(reactElement.props, previous.props)
    ) {
      return previous;
    }
  }

  previousRef.current = reactElement;
  return reactElement;
}
