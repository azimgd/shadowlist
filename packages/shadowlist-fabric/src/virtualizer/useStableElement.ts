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
 * Keeps the previous element while the new one describes the same thing (same component,
 * same key, shallow-equal props).
 *
 * Slot props are written inline at the call site -- `ItemSeparatorComponent={<Separator />}`
 * is the obvious way to pass one -- which hands the list a new element on every render of
 * the caller. That element is part of every row's content, so a new identity rebuilds the
 * whole mounted window even though nothing about the separator changed; on a grouped list
 * this measured as the single largest source of row re-renders. Callers should not have to
 * know that, so the list absorbs it here.
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
