import type { ActivityItem } from 'shadowlist-utils/native';
import { buildActivity } from '../fixtures/activity';
import { Collection, type CursorPage, type PageCursor } from './Collection';
import { request, type RequestSignal } from './network';

// Large enough that the screen's containerOffsetIndex (30) lands inside the first page.
const PAGE_SIZE = 50;
const NEW_ACTIVITY_PER_REFRESH = 10;

let generatedCount = 0;
const generateActivity = (count: number) =>
  Array.from({ length: count }, () => buildActivity(generatedCount++));

const activity = new Collection<ActivityItem>({
  seed: () => generateActivity(300),
  extend: generateActivity,
});

let headReads = 0;

export function fetchActivityPage(
  cursor: PageCursor | undefined,
  signal?: RequestSignal
): Promise<CursorPage<ActivityItem>> {
  return request(() => {
    if (cursor === undefined && headReads++ > 0) {
      activity.insertAtStart(generateActivity(NEW_ACTIVITY_PER_REFRESH));
    }
    return activity.page(cursor, { limit: PAGE_SIZE, from: 'start' });
  }, signal);
}

export function deleteActivities(ids: ReadonlyArray<string>): Promise<void> {
  return request(() => activity.remove(ids));
}
