import { forwardRef, useCallback, useMemo } from 'react';
import {
  ShadowList,
  type ShadowListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { useLabels } from '../labels';
import { ListHeader } from '../primitives/ListHeader';
import { PollFooter } from './PollFooter';
import { PollRow } from './PollRow';
import { defaultPollLabels, type PollLabels } from './labels';
import type { PollData, PollOption } from './types';

type RenderPollOption = NonNullable<ShadowListProps<PollOption>['renderItem']>;

export type PollListProps = Omit<
  ShadowListProps<PollOption>,
  'data' | 'renderItem'
> & {
  poll: PollData;
  renderItem?: RenderPollOption;
  onVote?: (optionId: string) => void;
  labels?: Partial<PollLabels>;
};

export const PollList = forwardRef<ShadowListCommands, PollListProps>(
  (
    {
      poll,
      renderItem,
      onVote,
      labels,
      ListHeaderComponent,
      ListFooterComponent,
      ...props
    },
    ref
  ) => {
    const { question, options, selectedId } = poll;
    const mergedLabels = useLabels(defaultPollLabels, labels);

    const { totalVotes, leadingId } = useMemo(() => {
      let total = 0;
      let leader: PollOption | undefined;
      for (const option of options) {
        total += option.votes;
        if (leader === undefined || option.votes > leader.votes) {
          leader = option;
        }
      }
      return { totalVotes: total, leadingId: leader?.id };
    }, [options]);

    // Rows re-render only when the totals, selection, labels or onVote change.
    const renderOption = useCallback<RenderPollOption>(
      ({ item }) => (
        <PollRow
          item={item}
          totalVotes={totalVotes}
          isLeading={item.id === leadingId}
          isSelected={item.id === selectedId}
          onVote={onVote}
          labels={mergedLabels}
        />
      ),
      [totalVotes, leadingId, selectedId, onVote, mergedLabels]
    );

    const header = useMemo(
      () =>
        ListHeaderComponent === undefined ? (
          <ListHeader title={question} />
        ) : (
          ListHeaderComponent
        ),
      [ListHeaderComponent, question]
    );
    const footer = useMemo(
      () =>
        ListFooterComponent === undefined ? (
          <PollFooter totalVotes={totalVotes} labels={mergedLabels} />
        ) : (
          ListFooterComponent
        ),
      [ListFooterComponent, totalVotes, mergedLabels]
    );

    return (
      <ShadowList
        ref={ref}
        data={options}
        stickyHeader
        stickyFooter
        ListHeaderComponent={header}
        ListFooterComponent={footer}
        renderItem={renderItem ?? renderOption}
        {...props}
      />
    );
  }
);
