import type { RelativeTimeLabels } from '../formatRelativeTime';

export interface ActivityLabels extends RelativeTimeLabels {
  unread: string;
}

export const defaultActivityLabels: ActivityLabels = {
  now: 'now',
  minutes: (count) => `${count}m`,
  hours: (count) => `${count}h`,
  days: (count) => `${count}d`,
  unread: 'Unread',
};
