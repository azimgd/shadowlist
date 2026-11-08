import { forwardRef, useCallback } from 'react';
import {
  ShadowList,
  type ContextMenu,
  type RenderItemInfo,
  type ShadowListCommands,
  type ShadowListProps,
  type SwipeActionsConfiguration,
} from 'shadowlist';
import { useLabels } from '../labels';
import { ItemSeparator } from '../primitives/ItemSeparator';
import { useTheme } from '../theme';
import { defaultInboxLabels, type InboxLabels } from './labels';
import { InboxRow } from './InboxRow';
import type { InboxMessage } from './types';

type ItemInfo = { item: InboxMessage; index: number };

/*
 * A handler may return a promise. A row swiped all the way stays out until it settles.
 */
type MessageAction = (item: InboxMessage) => void | Promise<unknown>;

export type InboxListProps = Omit<
  ShadowListProps<InboxMessage>,
  | 'renderItem'
  | 'allowsMultipleSelection'
  | 'leadingSwipeActionsForItem'
  | 'trailingSwipeActionsForItem'
  | 'contextMenuForItem'
> & {
  onPressItem?: (item: InboxMessage) => void;
  onToggleRead?: MessageAction;
  onToggleFlag?: MessageAction;
  onDelete?: MessageAction;
  onReply?: MessageAction;
  onSelectItem?: (item: InboxMessage) => void;
  editing?: boolean;
  labels?: Partial<InboxLabels>;
};

/*
 * A mail list on the list's own interaction features. Rows swipe natively: read or unread on
 * the leading side, delete and flag on the trailing side, and a full swipe runs the first
 * action. A long press opens a context menu. While editing a tap selects rows and swiping is
 * off. Separators hide next to a pressed row.
 */
export const InboxList = forwardRef<ShadowListCommands, InboxListProps>(
  (
    {
      onPressItem,
      onToggleRead,
      onToggleFlag,
      onDelete,
      onReply,
      onSelectItem,
      editing = false,
      labels,
      ...props
    },
    ref
  ) => {
    const { colors } = useTheme();
    const rowLabels = useLabels(defaultInboxLabels, labels);

    const renderItem = useCallback(
      ({
        item,
        selected,
        select,
        deselect,
        separators,
      }: RenderItemInfo<InboxMessage>) => (
        <InboxRow
          item={item}
          editing={editing}
          selected={selected}
          separators={separators}
          labels={rowLabels}
          onPress={editing ? (selected ? deselect : select) : onPressItem}
        />
      ),
      [editing, onPressItem, rowLabels]
    );

    const leadingSwipeActionsForItem = useCallback(
      ({ item }: ItemInfo): SwipeActionsConfiguration<InboxMessage> | null =>
        editing || !onToggleRead
          ? null
          : {
              actions: [
                {
                  title: item.read ? rowLabels.unread : rowLabels.read,
                  backgroundColor: colors.blue,
                  onPress: (info) => onToggleRead(info.item),
                },
              ],
            },
      [editing, onToggleRead, rowLabels, colors.blue]
    );

    const trailingSwipeActionsForItem = useCallback(
      ({ item }: ItemInfo): SwipeActionsConfiguration<InboxMessage> | null => {
        if (editing) return null;
        const actions: SwipeActionsConfiguration<InboxMessage>['actions'][number][] =
          [];
        if (onDelete) {
          actions.push({
            title: rowLabels.delete,
            style: 'destructive',
            onPress: (info) => onDelete(info.item),
          });
        }
        if (onToggleFlag) {
          actions.push({
            title: item.flagged ? rowLabels.unflag : rowLabels.flag,
            backgroundColor: colors.orange,
            onPress: (info) => onToggleFlag(info.item),
          });
        }
        return actions.length > 0 ? { actions } : null;
      },
      [editing, onDelete, onToggleFlag, rowLabels, colors.orange]
    );

    const contextMenuForItem = useCallback(
      ({ item }: ItemInfo): ContextMenu<InboxMessage> | null => {
        if (editing) return null;
        const actions: ContextMenu<InboxMessage>['actions'][number][] = [];
        if (onReply) {
          actions.push({
            title: rowLabels.reply,
            systemImage: 'arrowshape.turn.up.left',
            onPress: (info) => onReply(info.item),
          });
        }
        if (onToggleRead) {
          actions.push({
            title: item.read ? rowLabels.markUnread : rowLabels.markRead,
            systemImage: item.read ? 'envelope.badge' : 'envelope.open',
            onPress: (info) => onToggleRead(info.item),
          });
        }
        if (onToggleFlag) {
          actions.push({
            title: item.flagged ? rowLabels.unflag : rowLabels.flag,
            systemImage: item.flagged ? 'flag.slash' : 'flag',
            onPress: (info) => onToggleFlag(info.item),
          });
        }
        if (onSelectItem) {
          actions.push({
            title: rowLabels.select,
            systemImage: 'checkmark.circle',
            onPress: (info) => onSelectItem(info.item),
          });
        }
        if (onDelete) {
          actions.push({
            title: rowLabels.delete,
            style: 'destructive',
            systemImage: 'trash',
            onPress: (info) => onDelete(info.item),
          });
        }
        return actions.length > 0 ? { title: item.subject, actions } : null;
      },
      [
        editing,
        onReply,
        onToggleRead,
        onToggleFlag,
        onSelectItem,
        onDelete,
        rowLabels,
      ]
    );

    return (
      <ShadowList
        ref={ref}
        renderItem={renderItem}
        ItemSeparatorComponent={ItemSeparator}
        allowsMultipleSelection={editing}
        leadingSwipeActionsForItem={leadingSwipeActionsForItem}
        trailingSwipeActionsForItem={trailingSwipeActionsForItem}
        contextMenuForItem={contextMenuForItem}
        {...props}
      />
    );
  }
);
