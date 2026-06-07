import { forwardRef, useMemo } from 'react';
import {
  Shadowlist,
  type ShadowlistProps,
  type ShadowlistCommands,
} from 'shadowlist-wasm';
import { PollOptionRow } from './PollOption';
import type { PollOption } from './data';

export type PollListProps = Omit<ShadowlistProps<PollOption>, 'renderElement'> & {
  renderElement?: ShadowlistProps<PollOption>['renderElement'];
  // Called with the option id when a row is clicked.
  onVote?: (id: string) => void;
};

// A short interactive poll with sticky header/footer. Computes each option's
// share and the live leader from `data`; clicking a row fires `onVote`.
export const PollList = forwardRef<ShadowlistCommands, PollListProps>(
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
      <Shadowlist
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
