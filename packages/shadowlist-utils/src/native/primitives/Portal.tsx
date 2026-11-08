import {
  useLayoutEffect,
  useRef,
  useSyncExternalStore,
  type ReactNode,
} from 'react';
import { StyleSheet, View } from 'react-native';

/*
 * A place over everything else on screen that a component deep in the tree can draw into. A
 * phone has the system's own modal window for that. On a desktop the app mounts one PortalHost
 * last in its root view and the surfaces that must cover the window (sheets, menus) draw there
 * through `usePortal`.
 */
type Entries = ReadonlyMap<number, ReactNode>;

const listeners = new Set<() => void>();
let entries: Entries = new Map();
let hosts = 0;
let nextKey = 1;

function emit() {
  listeners.forEach((listener) => listener());
}

function subscribe(listener: () => void) {
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
  };
}

const getEntries = () => entries;

function setEntry(key: number, node: ReactNode | null) {
  if (node === null && !entries.has(key)) {
    return;
  }
  const next = new Map(entries);
  if (node === null) {
    next.delete(key);
  } else {
    next.set(key, node);
  }
  entries = next;
  emit();
}

/*
 * Draws the portaled surfaces, in the order they opened. Mount one, over everything.
 */
export function PortalHost() {
  useLayoutEffect(() => {
    hosts += 1;
    return () => {
      hosts -= 1;
    };
  }, []);
  const shown = useSyncExternalStore(subscribe, getEntries);
  if (shown.size === 0) {
    return null;
  }
  return (
    <View style={StyleSheet.absoluteFill} pointerEvents="box-none">
      {[...shown].map(([key, node]) => (
        <View
          key={key}
          style={StyleSheet.absoluteFill}
          pointerEvents="box-none"
        >
          {node}
        </View>
      ))}
    </View>
  );
}

/*
 * Draws `node` in the PortalHost for as long as the caller is mounted and `node` is not null.
 * Returns whether a host took it. Without one, the caller draws it in place.
 */
export function usePortal(node: ReactNode | null): boolean {
  const key = useRef(0);
  if (key.current === 0) {
    key.current = nextKey++;
  }
  useLayoutEffect(() => {
    setEntry(key.current, node);
  });
  useLayoutEffect(
    () => () => {
      setEntry(key.current, null);
    },
    []
  );
  return hosts > 0;
}
