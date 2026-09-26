import { memo, useCallback, useEffect, useRef, type ReactElement } from 'react';
import type { LayoutChangeEvent, ViewStyle } from 'react-native';
import { ShadowListElementView } from 'shadowlist';
import { countRowRender, slTrace, slTraceEnabled } from './helpers';

/*
 * Per list row index state shared by every row. Rows keep their children across moves, and
 * the memo check below skips a row whose only change is its index, unless the row read it.
 * keyToIndex answers index reads made after render, like from a press handler, for rows
 * that skipped a move and still hold an old index.
 */
export interface RowIndexStore {
  keyToIndex: ReadonlyMap<string, number>;
  // Keys of mounted rows whose renderElement read the index.
  readers: Set<string>;
}

export function createRowIndexStore(): RowIndexStore {
  return { keyToIndex: new Map(), readers: new Set() };
}

interface ElementRendererProps<ElementT> {
  element: ElementT;
  index: number;
  rowIndex: RowIndexStore;
  elementKey: string;
  style: ViewStyle | ViewStyle[];
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  separator: ReactElement | null;
  nativeIndex: number;
  /*
   * Set only when trackElementSizes is on. Otherwise the row has no onLayout at all, so
   * lists that don't track sizes pay nothing.
   */
  onElementLayout?: (key: string, width: number, height: number) => void;
  onElementRelease?: (key: string) => void;
}

interface RenderedChildren<ElementT> {
  element: ElementT;
  index: number;
  renderElement: ElementRendererProps<ElementT>['renderElement'];
  separator: ReactElement | null;
  readIndex: boolean;
  children: ReactElement;
}

/*
 * Props equal except maybe index, and the index only counts for a row that read it. A prepend
 * shifts every mounted row's index, and without this every row ran again just to hand the
 * same children back. Keep this in sync with ElementRendererProps.
 */
function sameRowProps<ElementT>(
  previous: ElementRendererProps<ElementT>,
  next: ElementRendererProps<ElementT>
): boolean {
  return (
    previous.element === next.element &&
    previous.elementKey === next.elementKey &&
    previous.style === next.style &&
    previous.renderElement === next.renderElement &&
    previous.separator === next.separator &&
    previous.nativeIndex === next.nativeIndex &&
    previous.onElementLayout === next.onElementLayout &&
    previous.onElementRelease === next.onElementRelease &&
    previous.rowIndex === next.rowIndex &&
    (previous.index === next.index ||
      !next.rowIndex.readers.has(next.elementKey))
  );
}

export const ElementRenderer = memo(function ElementRendererInner<
  ElementT extends { id: string },
>({
  element,
  index,
  rowIndex,
  elementKey,
  style,
  renderElement,
  separator,
  nativeIndex,
  onElementLayout,
  onElementRelease,
}: ElementRendererProps<ElementT>) {
  /*
   * Rebuild the row only when something it used changed. The index only counts if the last
   * renderElement call read it. A prepend shifts every row's index but not its element, so a
   * row that never read the index can keep its children and React skips the subtree.
   * A renderer that does read the index, say for numbering, still re-renders when it moves.
   */
  const renderedRef = useRef<RenderedChildren<ElementT> | null>(null);
  /*
   * The getter returns the row's current index and marks it as read, even after render, like
   * from a press handler. Such a row re-renders on its next move. A late read looks the key
   * up, since a row that never read the index skips moves and its own index gets old.
   */
  const indexRef = useRef(index);
  indexRef.current = index;
  const rendered = renderedRef.current;
  let children: ReactElement;
  if (
    rendered !== null &&
    rendered.element === element &&
    rendered.renderElement === renderElement &&
    rendered.separator === separator &&
    (!rendered.readIndex || rendered.index === index)
  ) {
    children = rendered.children;
  } else {
    countRowRender();
    if (rendered !== null && slTraceEnabled()) {
      slTrace(
        `row-miss key=${elementKey} element=${rendered.element !== element ? 1 : 0}` +
          ` render=${rendered.renderElement !== renderElement ? 1 : 0}` +
          ` separator=${rendered.separator !== separator ? 1 : 0}` +
          ` index=${rendered.readIndex && rendered.index !== index ? 1 : 0}`
      );
    }
    const next: RenderedChildren<ElementT> = {
      element,
      index,
      renderElement,
      separator,
      readIndex: false,
      children: null as unknown as ReactElement,
    };
    let rendering = true;
    const content = renderElement({
      element,
      get index() {
        next.readIndex = true;
        rowIndex.readers.add(elementKey);
        if (rendering) return indexRef.current;
        return rowIndex.keyToIndex.get(elementKey) ?? indexRef.current;
      },
    });
    rendering = false;
    next.children = (
      <>
        {content}
        {separator}
      </>
    );
    renderedRef.current = next;
    children = next.children;
  }

  const handleLayout = useCallback(
    (event: LayoutChangeEvent) => {
      const { width, height } = event.nativeEvent.layout;
      onElementLayout?.(elementKey, width, height);
    },
    [onElementLayout, elementKey]
  );

  /*
   * Reader marks are only added during render, since a thrown away render could otherwise
   * clear the mark of the committed one. Unmount clears it, and a remount, like StrictMode's,
   * adds it back.
   */
  useEffect(() => {
    if (renderedRef.current?.readIndex) rowIndex.readers.add(elementKey);
    return () => {
      rowIndex.readers.delete(elementKey);
    };
  }, [rowIndex, elementKey]);

  // Forget the row's size on unmount. If the key comes back it gets measured again.
  useEffect(() => {
    if (!onElementRelease) return;
    return () => onElementRelease(elementKey);
  }, [onElementRelease, elementKey]);

  return (
    <ShadowListElementView
      index={nativeIndex}
      elementKey={elementKey}
      style={style}
      onLayout={onElementLayout ? handleLayout : undefined}
    >
      {children}
    </ShadowListElementView>
  );
}, sameRowProps) as <ElementT extends { id: string }>(
  props: ElementRendererProps<ElementT>
) => ReactElement;
