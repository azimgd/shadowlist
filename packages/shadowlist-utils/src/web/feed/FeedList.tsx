import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist-wasm';
import type { FeedItem } from 'shadowlist-utils';
import { FeedElement } from './FeedElement';

// `data` + Shadowlist props, with `renderElement` defaulted to the Feed row.
export type FeedListProps = Omit<ShadowlistProps<FeedItem>, 'renderElement'> & {
  renderElement?: ShadowlistProps<FeedItem>['renderElement'];
};

const renderFeedElement: ShadowlistProps<FeedItem>['renderElement'] = ({
  element,
  index,
}) => <FeedElement element={element} index={index} />;

// A vertical feed list of avatar/text/image rows. Drop in `data` to use.
export const FeedList = forwardRef<ShadowlistCommands, FeedListProps>(
  ({ renderElement, ...props }, ref) => (
    <Shadowlist
      ref={ref}
      renderElement={renderElement ?? renderFeedElement}
      {...props}
    />
  )
);
