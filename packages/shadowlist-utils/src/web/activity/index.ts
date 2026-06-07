import { ActivityList } from './ActivityList';
import { ActivityRow } from './ActivityRow';
import { ActivityHeader } from './ActivityHeader';

export type { ActivityListProps } from './ActivityList';
export type { ActivityRowProps } from './ActivityRow';
export type { ActivityHeaderProps, ActivityHeaderAction } from './ActivityHeader';

// Activity domain: <Activity.List /> + <Activity.Row /> + <Activity.Header />.
export const Activity = {
  List: ActivityList,
  Row: ActivityRow,
  Header: ActivityHeader,
};
