import { forwardRef, useCallback } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { useLabels } from '../labels';
import { FeedRow, type FeedRowProps } from './FeedRow';
import { defaultFeedLabels } from './labels';
import type { FeedItem } from './types';

type RenderFeedItem = NonNullable<ShadowListProps<FeedItem>['renderItem']>;

export type FeedListProps = Omit<ShadowListProps<FeedItem>, 'renderItem'> &
  Pick<FeedRowProps, 'onPressImage' | 'formatTime' | 'labels'> & {
    renderItem?: RenderFeedItem;
    onPressItem?: (item: FeedItem) => void;
  };

export const FeedList = forwardRef<ShadowListCommands, FeedListProps>(
  (
    { renderItem, onPressItem, onPressImage, formatTime, labels, ...props },
    ref
  ) => {
    const rowLabels = useLabels(defaultFeedLabels, labels);
    const renderRow = useCallback<RenderFeedItem>(
      ({ item }) => (
        <FeedRow
          item={item}
          onPress={onPressItem}
          onPressImage={onPressImage}
          formatTime={formatTime}
          labels={rowLabels}
        />
      ),
      [onPressItem, onPressImage, formatTime, rowLabels]
    );
    return (
      <ShadowList
        ref={ref}
        autoHideHeader
        renderItem={renderItem ?? renderRow}
        {...props}
      />
    );
  }
);
