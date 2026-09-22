import { useCallback, useSyncExternalStore } from 'react';
import type { AssistantTurn } from './types';

/*
 * Holds turns while they stream. The list data changes once when a reply starts and once
 * when it ends. In between only the streaming row reads from here, so a token costs one
 * row render and no list work.
 */
export interface AssistantStreamStore {
  subscribe: (listener: () => void) => () => void;
  get: (messageId: string) => AssistantTurn | undefined;
  set: (messageId: string, turn: AssistantTurn) => void;
  remove: (messageId: string) => void;
}

export function createStreamStore(): AssistantStreamStore {
  const turns = new Map<string, AssistantTurn>();
  const listeners = new Set<() => void>();
  const notify = () => listeners.forEach((listener) => listener());

  return {
    subscribe: (listener) => {
      listeners.add(listener);
      return () => {
        listeners.delete(listener);
      };
    },
    get: (messageId) => turns.get(messageId),
    set: (messageId, turn) => {
      turns.set(messageId, turn);
      notify();
    },
    remove: (messageId) => {
      if (turns.delete(messageId)) notify();
    },
  };
}

/*
 * Every mounted reply subscribes, but only the row whose snapshot changed re-renders,
 * so a flush wakes one row.
 */
export function useStreamingTurn(
  store: AssistantStreamStore,
  messageId: string
): AssistantTurn | undefined {
  const getSnapshot = useCallback(
    () => store.get(messageId),
    [store, messageId]
  );
  return useSyncExternalStore(store.subscribe, getSnapshot);
}
