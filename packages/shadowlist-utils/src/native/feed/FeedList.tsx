import { forwardRef } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist';
import type { FeedItem } from 'shadowlist-utils';
import { FeedElement } from './FeedElement';

// `data` + Shadowlist props, with `renderElement` made optional (defaults to
// the Feed row). Pass any Shadowlist prop to override a baked-in default.
export type FeedListProps = Omit<ShadowlistProps<FeedItem>, 'renderElement'> & {
  renderElement?: ShadowlistProps<FeedItem>['renderElement'];
};

// Module-level so the default keeps a stable identity across renders (lets
// Shadowlist skip re-rendering unchanged rows).
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
