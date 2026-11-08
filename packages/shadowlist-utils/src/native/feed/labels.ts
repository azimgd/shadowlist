import type { RelativeTimeLabels } from '../formatRelativeTime';

/*
 * loading is read for a placeholder row while its post loads.
 */
export interface FeedLabels extends RelativeTimeLabels {
  image: string;
  loading: string;
}

export const defaultFeedLabels: FeedLabels = {
  now: 'now',
  minutes: (count) => `${count}m`,
  hours: (count) => `${count}h`,
  days: (count) => `${count}d`,
  image: 'Image',
  loading: 'Loading post',
};
