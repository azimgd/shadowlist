import type { AssistantEndMarker } from './types';

export const ASSISTANT_END_ID = 'assistant-end';

/*
 * Append after the last message. It is invisible but a real row, so "the end marker is
 * viewable" means "the bottom is on screen".
 */
export const ASSISTANT_END_MARKER: AssistantEndMarker = {
  id: ASSISTANT_END_ID,
  role: 'end',
};

export const ASSISTANT_END_MARKER_HEIGHT = 1;
