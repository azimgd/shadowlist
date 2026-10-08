import type { AssistantStreamingTurn } from './types';

export function emptyTurn(): AssistantStreamingTurn {
  return { status: 'streaming', content: '' };
}
