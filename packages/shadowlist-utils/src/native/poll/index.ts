import { PollList } from './PollList';
import { PollOptionRow } from './PollOption';

export type { PollListProps } from './PollList';
export type { PollOptionProps } from './PollOption';
export type { PollOption, IconComponent } from './data';
export { buildOption, buildPoll } from './data';

export const Poll = {
  List: PollList,
  Option: PollOptionRow,
};
