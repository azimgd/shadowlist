import { useCallback, useRef, type ComponentRef } from 'react';
import ShadowListCellView from './ShadowListCellViewNativeComponent';
import type { ItemActionsViewProps } from './types';
import { useCellActions } from './virtualizer/useCellActions';

/*
 * Index the handlers get. The view is not a row of a list.
 */
const NO_INDEX = -1;

/*
 * A view outside a list with the swipe actions and menu of a list row, like a row of an
 * inset grouped card. It swipes like a row of a vertical list and uses the same native cell.
 */
export default function ItemActionsView<ItemT = undefined>({
  item,
  leadingSwipeActions,
  trailingSwipeActions,
  contextMenu,
  children,
  ...props
}: ItemActionsViewProps<ItemT>) {
  const cellViewRef = useRef<ComponentRef<typeof ShadowListCellView> | null>(
    null
  );
  const currentIndex = useCallback(() => NO_INDEX, []);
  const actionProps = useCellActions({
    item: item as ItemT,
    currentIndex,
    leadingSwipe: leadingSwipeActions,
    trailingSwipe: trailingSwipeActions,
    menu: contextMenu,
    cellViewRef,
  });
  return (
    <ShadowListCellView ref={cellViewRef} {...props} index={0} {...actionProps}>
      {children}
    </ShadowListCellView>
  );
}
