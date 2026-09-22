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
 * Keeps the old element while the new one is the same component, key and props.
 *
 * Callers pass elements like the separator inline, which makes a new one every render. It
 * sits inside every row, so each new one rebuilds all mounted rows. On a grouped list this
 * was the biggest source of row re-renders, so the list handles it here.
 */
export function useStableElement(
  element: ReactElement | null
): ReactElement | null {
  const previousRef = useRef<ReactElement | null>(null);
  const previous = previousRef.current;

  if (element !== null && previous !== null && element !== previous) {
    if (
      element.type === previous.type &&
      element.key === previous.key &&
      shallowEqualProps(element.props, previous.props)
    ) {
      return previous;
    }
  }

  previousRef.current = element;
  return element;
}
