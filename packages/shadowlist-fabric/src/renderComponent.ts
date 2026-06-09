import type { ReactElement } from 'react';

/* Resolve a list slot prop that may be an element, a factory, or absent. */
export const renderComponent = (
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null => {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
};
