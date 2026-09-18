import type { RelativeTimeLabels } from '../formatRelativeTime';

export interface FeedLabels extends RelativeTimeLabels {
  image: string;
}

export const defaultFeedLabels: FeedLabels = {
  now: 'now',
  minutes: (count) => `${count}m`,
  hours: (count) => `${count}h`,
  days: (count) => `${count}d`,
  image: 'Image',
};
