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

type RenderFeedItem = NonNullable<ShadowListProps<FeedItem>['renderElement']>;

export type FeedListProps = Omit<ShadowListProps<FeedItem>, 'renderElement'> &
  Pick<FeedRowProps, 'onPressImage' | 'formatTime' | 'labels'> & {
    renderElement?: RenderFeedItem;
    onPressItem?: (item: FeedItem) => void;
  };

export const FeedList = forwardRef<ShadowListCommands, FeedListProps>(
  (
    { renderElement, onPressItem, onPressImage, formatTime, labels, ...props },
    ref
  ) => {
    const rowLabels = useLabels(defaultFeedLabels, labels);
    const renderRow = useCallback<RenderFeedItem>(
      ({ element }) => (
        <FeedRow
          item={element}
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
        renderElement={renderElement ?? renderRow}
        {...props}
      />
    );
  }
);
