import { useCallback, useSyncExternalStore } from 'react';
import type { AssistantTurn } from './types';

/*
 * The store holds turns while they stream. The list's `data` is committed once when a reply
 * starts and once when it ends; in between, only the streaming row reads from here
 * (useStreamingTurn). A token therefore costs one row render and no list work at all: no
 * new data identity, no key re-extraction, no measure-spec rebuild.
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
 * Every mounted reply subscribes, but useSyncExternalStore re-renders only a row whose own
 * snapshot changed, so a flush wakes exactly one row.
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
