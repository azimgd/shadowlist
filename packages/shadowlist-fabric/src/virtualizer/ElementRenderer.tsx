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
   * Set only when the list tracks row sizes (ShadowListProps.trackElementSizes). Left
   * undefined the row renders without an onLayout at all, so an untracked list pays
   * nothing.
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
   * The row's content is rebuilt only when something it used changed. `index` counts only if
   * the last renderElement call read it: a prepend or insert moves every mounted row's index
   * while its element stays the same object, and a row whose content never looked at the
   * index renders the same output at the new one. Keeping the children's identity lets React
   * skip the whole subtree. A renderer that reads index (numbering, index-derived styling)
   * still re-renders when it moves.
   */
  const renderedRef = useRef<RenderedChildren<ElementT> | null>(null);
  /*
   * The getter reads the row's CURRENT index and marks the cache whenever it is read, including
   * after render (a press handler holding on to the info object): such a row re-renders on its
   * next move, and until then the late read still returns the right index.
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

  // Drop the row's recorded size on unmount; a key that comes back re-measures anyway.
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
