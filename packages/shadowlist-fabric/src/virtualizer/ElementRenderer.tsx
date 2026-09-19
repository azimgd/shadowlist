import { memo, useRef, type ReactElement } from 'react';
import type { ViewStyle } from 'react-native';
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

  return (
    <ShadowListElementView
      index={nativeIndex}
      elementKey={elementKey}
      style={style}
    >
      {children}
    </ShadowListElementView>
  );
}) as <ElementT extends { id: string }>(
  props: ElementRendererProps<ElementT>
) => ReactElement;
