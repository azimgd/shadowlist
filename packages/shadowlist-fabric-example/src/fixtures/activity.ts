import type { ActivityItem } from 'shadowlist-utils/native';
import {
  CHARACTER_NAMES,
  generateRandomText,
  generateUniqueId,
} from './common';

export const ACTIVITY_ACTIONS = [
  'started following you',
  'liked your trip photo',
  'tagged you in a check-in',
  'added you to a trip crew',
  'commented on your itinerary',
  'is boarding at gate 22',
];

const MINUTE_MS = 60_000;

export function buildActivity(index: number): ActivityItem {
  return {
    id: generateUniqueId(),
    actor: { name: CHARACTER_NAMES[index % CHARACTER_NAMES.length]! },
    action: ACTIVITY_ACTIONS[index % ACTIVITY_ACTIONS.length]!,
    text: generateRandomText(index).slice(0, 90),
    createdAt: Date.now() - ((index % 59) + 1) * MINUTE_MS,
  };
}

// Threshold presets the Activity header cycles through, as a fraction of the visible length.
export const START_REACHED_THRESHOLDS = [0.5, 1, 2];
export const END_REACHED_THRESHOLDS = [0.5, 1.5, 3];

export function nextInCycle(steps: number[], current: number): number {
  return steps[(steps.indexOf(current) + 1) % steps.length]!;
}
