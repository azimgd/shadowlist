import type { AssistantToolCall } from './types';

type Step =
  | { kind: 'text'; text: string; at: number }
  | { kind: 'calls'; calls: AssistantToolCall[] };

/*
 * Splits the reply text where each tool call was made and puts the calls between the pieces.
 * A call with no saved position, from an older transcript, goes first. Blank pieces are
 * dropped, since the card already leaves a gap.
 *
 * Calls made back to back, with no text between them, are one step: they draw as one grouped
 * card with a row per call, the way iOS lists a run of related rows in an inset group, rather
 * than as a stack of separate cards.
 */
export function stepsOf(
  content: string,
  calls: readonly AssistantToolCall[]
): Step[] {
  const steps: Step[] = [];
  let cursor = 0;
  const pushText = (end: number) => {
    const text = content.slice(cursor, end);
    if (text.trim() !== '')
      steps.push({ kind: 'text', text: text.trim(), at: cursor });
    cursor = end;
  };
  const ordered = calls
    .map((call, index) => ({
      call,
      index,
      at: Math.min(call.at ?? 0, content.length),
    }))
    .sort((a, b) => a.at - b.at || a.index - b.index);
  for (const { call, at } of ordered) {
    if (at > cursor) pushText(at);
    const last = steps[steps.length - 1];
    if (last !== undefined && last.kind === 'calls') last.calls.push(call);
    else steps.push({ kind: 'calls', calls: [call] });
  }
  pushText(content.length);
  return steps;
}
