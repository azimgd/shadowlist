import { forwardRef, useMemo } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { PollOptionRow } from './PollOption';
import type { PollOption } from './data';

export type PollListProps = Omit<
  ShadowListProps<PollOption>,
  'renderElement'
> & {
  renderElement?: ShadowListProps<PollOption>['renderElement'];
  // Called with the option key when a row is tapped.
  onVote?: (key: string) => void;
};

/*
 * A short, interactive poll list with a sticky header + footer. Computes each
 * option's share and the live leader from `data`; tapping a row fires `onVote`.
 * Drive votes by updating your `data` state in the `onVote` handler.
 */
export const PollList = forwardRef<ShadowListCommands, PollListProps>(
  ({ renderElement, onVote, data, ...props }, ref) => {
    const total = useMemo(
      () => data.reduce((sum, option) => sum + option.votes, 0),
      [data]
    );
    const leadingId = useMemo(
      () =>
        data.length === 0
          ? undefined
          : data.reduce(
              (best, option) => (option.votes > best.votes ? option : best),
              data[0]!
            ).id,
      [data]
    );

    return (
      <ShadowList
        ref={ref}
        data={data}
        stickyHeader
        stickyFooter
        keyExtractor={(item) => item.id}
        renderElement={
          renderElement ??
          (({ element }) => (
            <PollOptionRow
              option={element}
              total={total}
              leading={element.id === leadingId}
              onVote={onVote}
            />
          ))
        }
        {...props}
      />
    );
  }
);
