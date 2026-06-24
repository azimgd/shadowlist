import { forwardRef } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import type { FeedItem } from 'shadowlist-utils';
import { FeedElement } from './FeedElement';

// `data` + ShadowList props, with `renderElement` made optional (defaults to
// the Feed row). Pass any ShadowList prop to override a baked-in default.
export type FeedListProps = Omit<ShadowListProps<FeedItem>, 'renderElement'> & {
  renderElement?: ShadowListProps<FeedItem>['renderElement'];
};

// Module-level so the default keeps a stable identity across renders (lets
// ShadowList skip re-rendering unchanged rows).
const renderFeedElement: ShadowListProps<FeedItem>['renderElement'] = ({
  element,
}) => <FeedElement element={element} />;

/*
 * A vertical feed list: themed avatar/text/image rows, auto-hiding header.
 * Drop in `data` to get a working feed; override `renderElement` or any other
 * ShadowList prop to customize.
 */
export const FeedList = forwardRef<ShadowListCommands, FeedListProps>(
  ({ renderElement, ...props }, ref) => (
    <ShadowList
      ref={ref}
      autoHideHeader
      renderElement={renderElement ?? renderFeedElement}
      {...props}
    />
  )
);
