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

/*
 * A vertical feed list: themed avatar/text/image rows, auto-hiding header.
 * Drop in `data` to get a working feed; override `renderElement` or any other
 * Shadowlist prop to customize.
 */
export const FeedList = forwardRef<ShadowlistCommands, FeedListProps>(
  ({ renderElement, ...props }, ref) => (
    <Shadowlist
      ref={ref}
      autoHideHeader
      renderElement={renderElement ?? renderFeedElement}
      {...props}
    />
  )
);
