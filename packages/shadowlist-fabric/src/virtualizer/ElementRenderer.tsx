import { memo, useCallback, useEffect, useRef, type ReactElement } from 'react';
import type { LayoutChangeEvent, ViewStyle } from 'react-native';
import { ShadowListElementView } from 'shadowlist';
import { countRowRender, slTrace, slTraceEnabled } from './helpers';

interface ElementRendererProps<ElementT> {
  element: ElementT;
  index: number;
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

export const ElementRenderer = memo(function ElementRendererInner<
  ElementT extends { id: string },
>({
  element,
  index,
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
   * from a press handler. Such a row re-renders on its next move, and a late read still gets
   * the right index.
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
    const content = renderElement({
      element,
      get index() {
        next.readIndex = true;
        return indexRef.current;
      },
    });
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
}) as <ElementT extends { id: string }>(
  props: ElementRendererProps<ElementT>
) => ReactElement;
