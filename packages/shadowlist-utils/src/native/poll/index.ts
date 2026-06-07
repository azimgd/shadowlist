import { PollList } from './PollList';
import { PollOptionRow } from './PollOption';

export type { PollListProps } from './PollList';
export type { PollOptionProps } from './PollOption';
export type { PollOption, IconComponent } from './data';
export { OPTION_SEEDS, buildOption, buildPoll } from './data';

/* Poll domain: <Poll.List data={options} onVote={...} /> + <Poll.Option />. */
export const Poll = {
  List: PollList,
  Option: PollOptionRow,
};
