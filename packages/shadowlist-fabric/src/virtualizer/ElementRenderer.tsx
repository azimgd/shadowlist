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
  const children = useMemo(
    () => (
      <>
        {renderElement({ element, index })}
        {separator}
      </>
    ),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [element, renderElement, separator]
  );

  return (
    <ShadowListElementView index={index} elementKey={elementKey} style={style}>
      {children}
    </ShadowListElementView>
  );
}) as <T extends { id: string }>(
  props: ElementRendererProps<T>
) => ReactElement;
