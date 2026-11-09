const MINUTE_MS = 60_000;
const HOUR_MS = 60 * MINUTE_MS;
const DAY_MS = 24 * HOUR_MS;
const WEEK_MS = 7 * DAY_MS;

export interface RelativeTimeLabels {
  now: string;
  minutes: (count: number) => string;
  hours: (count: number) => string;
  days: (count: number) => string;
}

export function formatRelativeTime(
  date: Date | number,
  labels: RelativeTimeLabels,
  nowMs: number = Date.now()
): string {
  const dateMs = typeof date === 'number' ? date : date.getTime();
  const elapsedMs = Math.max(0, nowMs - dateMs);
  if (elapsedMs < MINUTE_MS) {
    return labels.now;
  }
  if (elapsedMs < HOUR_MS) {
    return labels.minutes(Math.floor(elapsedMs / MINUTE_MS));
  }
  if (elapsedMs < DAY_MS) {
    return labels.hours(Math.floor(elapsedMs / HOUR_MS));
  }
  if (elapsedMs < WEEK_MS) {
    return labels.days(Math.floor(elapsedMs / DAY_MS));
  }
  return new Date(dateMs).toLocaleDateString(undefined, {
    month: 'short',
    day: 'numeric',
  });
}
