import { useCallback, useState } from 'react';

/*
 * `Pressable` reports only `pressed` to its style callback. A hover tint needs the hover
 * callbacks and a little state of its own. A window loses its key state while a menu is open;
 * hover has to survive that, which it does because these are plain pointer events.
 */
export function useHover(): [
  boolean,
  { onHoverIn: () => void; onHoverOut: () => void },
] {
  const [hovered, setHovered] = useState(false);
  const onHoverIn = useCallback(() => setHovered(true), []);
  const onHoverOut = useCallback(() => setHovered(false), []);
  return [hovered, { onHoverIn, onHoverOut }];
}
