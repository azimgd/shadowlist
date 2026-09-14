import { forwardRef, useCallback, useMemo } from 'react';
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

    /*
     * Recreated only when total/leadingId/onVote change, so ElementRenderer's per-row
     * memoization (keyed on renderElement identity) isn't defeated by every re-render of
     * this wrapper (e.g. from an unrelated prop change).
     */
    const defaultRenderElement = useCallback<
      NonNullable<ShadowListProps<PollOption>['renderElement']>
    >(
      ({ element }) => (
        <PollOptionRow
          option={element}
          total={total}
          leading={element.id === leadingId}
          onVote={onVote}
        />
      ),
      [total, leadingId, onVote]
    );

    return (
      <ShadowList
        ref={ref}
        data={data}
        stickyHeader
        stickyFooter
        keyExtractor={(option) => option.id}
        renderElement={renderElement ?? defaultRenderElement}
        {...props}
      />
    );
  }
);
