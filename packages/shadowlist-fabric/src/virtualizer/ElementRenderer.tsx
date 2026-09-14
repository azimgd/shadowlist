import { memo, useMemo, type ReactElement } from 'react';
import type { ViewStyle } from 'react-native';
import { ShadowListElementView } from 'shadowlist';

interface ElementRendererProps<ElementT> {
  element: ElementT;
  index: number;
  elementKey: string;
  style: ViewStyle | ViewStyle[];
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  separator: ReactElement | null;
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
}: ElementRendererProps<ElementT>) {
  /*
   * `index` is passed to renderElement, so it has to be a dependency: a prepend or
   * reorder keeps the same item object but moves it, and without this the row would keep
   * rendering content built from its old index (stale numbering, wrong separators,
   * index-derived styling) until something else invalidated the memo.
   */
  const children = useMemo(
    () => (
      <>
        {renderElement({ element, index })}
        {separator}
      </>
    ),
    [element, index, renderElement, separator]
  );

  return (
    <ShadowListElementView index={index} elementKey={elementKey} style={style}>
      {children}
    </ShadowListElementView>
  );
}) as <ElementT extends { id: string }>(
  props: ElementRendererProps<ElementT>
) => ReactElement;
