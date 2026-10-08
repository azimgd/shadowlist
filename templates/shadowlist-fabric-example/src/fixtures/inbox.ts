import type { InboxMessage } from 'shadowlist-utils/native';
import { CHARACTER_NAMES, generateUniqueId } from './common';

const SUBJECTS = [
  'Gate change for the 6:40 to Reykjavik',
  'Your seat upgrade request',
  'Photos from the aurora night',
  'Itinerary for the coast trip',
  'Re: Packing list',
  'Lounge access this weekend',
  'Boarding pass inside',
  'Dinner on the last night?',
  'Weather looks clear for Thursday',
  'Receipt for your booking',
];

const PREVIEWS = [
  'Boarding moved to B14 and starts twenty minutes later than planned. Grab a coffee on the way.',
  'Good news, there is room up front. Confirm before midnight and the upgrade is yours.',
  'I finally sorted through the shots from the ridge. The green band over the lake came out best.',
  'Day one is the drive along the cliffs, day two the lighthouse, day three nothing at all.',
  'Adding a rain shell and the small tripod. Anything else you want me to bring along?',
  'The lounge by gate C opens at five. Show the card at the desk and they will let two in.',
  'Here is your pass for tomorrow. Keep it handy, the scanner at security is slow.',
  'There is a tiny place by the harbour that does grilled fish. Want me to book a table?',
  'Low cloud clears by noon, light wind from the southwest. Perfect for the walk out.',
  'Thanks for booking with us. Your total and the cancellation terms are below.',
];

const MINUTE_MS = 60_000;

/*
 * The index-th message, newest first. Each one is a little older than the one before.
 */
export function generateInboxMessage(
  index: number,
  now: number = Date.now()
): InboxMessage {
  return {
    id: generateUniqueId(),
    sender: CHARACTER_NAMES[(index * 5) % CHARACTER_NAMES.length]!,
    subject: SUBJECTS[index % SUBJECTS.length]!,
    preview: PREVIEWS[(index * 3) % PREVIEWS.length]!,
    receivedAt: now - index * 37 * MINUTE_MS,
    read: index % 3 !== 0,
    flagged: index % 11 === 4,
  };
}
