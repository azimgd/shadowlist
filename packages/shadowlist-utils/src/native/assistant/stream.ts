import { useCallback, useSyncExternalStore } from 'react';
import { generateUniqueId } from 'shadowlist-utils';
import {
  emptyTurn,
  type AssistantScript,
  type AssistantTurn,
  type AssistantTurnStatus,
} from './data';

/*
 * Streaming for the assistant template, in two halves.
 *
 * The store holds turns while they stream. The list's `data` is committed once when a
 * reply starts and once when it ends; in between, only the streaming row reads from here
 * (useStreamingTurn). A token therefore costs one row render and no list work at all: no
 * new data identity, no key re-extraction, no measure-spec rebuild, and the core's
 * props-identity shortcut for the key set stays intact.
 *
 * The player turns a script into timed token events and coalesces them. Tokens mutate a
 * private draft; a fixed-cadence flush publishes a snapshot only when something changed,
 * so renders are capped at 1000 / STREAM_FLUSH_MS a second however fast tokens arrive.
 */

// ~20 renders a second: the flush cadence common chat SDKs default to.
const STREAM_FLUSH_MS = 50;

// Per-event delays (ms) before jitter: a thinking token, a content token, a tool round trip.
const FIRST_EVENT_MS = 350;
const THINKING_TOKEN_MS = 14;
const CONTENT_TOKEN_MS = 22;
const TOOL_START_MS = 160;
const TOOL_CALL_MS = 900;

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
 * The in-flight turn for one message, or undefined when it isn't streaming. Every mounted
 * reply subscribes, but useSyncExternalStore re-renders only a row whose own snapshot
 * changed, so a flush wakes exactly one row.
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

export interface StreamHandle {
  // Ends the stream now, keeping whatever text already arrived.
  stop: () => void;
}

export interface PlayScriptOptions {
  // Play the script's reasoning before its answer.
  thinking: boolean;
  // Coalesced snapshots, at most one per STREAM_FLUSH_MS.
  onUpdate: (turn: AssistantTurn) => void;
  // Exactly once, with the final turn: done, stopped or error.
  onFinish: (turn: AssistantTurn) => void;
}

type StreamEvent =
  | { kind: 'thinking'; text: string }
  | { kind: 'toolStart'; name: string; input: string }
  | { kind: 'toolEnd'; output: string; failed: boolean }
  | { kind: 'content'; text: string }
  | { kind: 'fail'; error: string };

// Word-sized chunks that keep their whitespace, so code indentation and newlines survive.
const tokenize = (text: string) => text.match(/\s+|\S+/g) ?? [];

const buildEvents = (script: AssistantScript, thinking: boolean) => {
  const events: StreamEvent[] = [];

  if (thinking && script.thinking) {
    for (const text of tokenize(script.thinking)) {
      events.push({ kind: 'thinking', text });
    }
  }

  for (const call of script.toolCalls ?? []) {
    events.push({ kind: 'toolStart', name: call.name, input: call.input });
    events.push({
      kind: 'toolEnd',
      output: call.output,
      failed: call.fails === true,
    });
  }

  const tokens = tokenize(script.content);
  const cutoff =
    script.failAt === undefined
      ? tokens.length
      : Math.floor(tokens.length * script.failAt);
  for (const text of tokens.slice(0, cutoff)) {
    events.push({ kind: 'content', text });
  }

  if (script.failAt !== undefined) {
    events.push({
      kind: 'fail',
      error: script.error ?? 'Something went wrong.',
    });
  }

  return events;
};

const delayBefore = (event: StreamEvent) => {
  switch (event.kind) {
    case 'thinking':
      return THINKING_TOKEN_MS;
    case 'toolStart':
      return TOOL_START_MS;
    case 'toolEnd':
      return TOOL_CALL_MS;
    default:
      return CONTENT_TOKEN_MS;
  }
};

// 0.5x..1.5x, so tokens arrive in the uneven bursts a network actually delivers.
const jitter = (ms: number) => Math.round(ms * (0.5 + Math.random()));

export function playScript(
  script: AssistantScript,
  { thinking, onUpdate, onFinish }: PlayScriptOptions
): StreamHandle {
  const events = buildEvents(script, thinking);
  const startedAt = Date.now();
  /*
   * Mutated in place per token; arrays inside it are replaced rather than mutated, so a
   * shallow copy is a valid snapshot and untouched tool calls keep their identity for memo.
   */
  const draft = emptyTurn();
  let cursor = 0;
  let dirty = false;
  let finished = false;
  let eventTimer: ReturnType<typeof setTimeout> | null = null;

  const flushTimer = setInterval(() => {
    if (!dirty) return;
    dirty = false;
    onUpdate({ ...draft });
  }, STREAM_FLUSH_MS);

  const finish = (status: AssistantTurnStatus, error = '') => {
    if (finished) return;
    finished = true;
    if (eventTimer) clearTimeout(eventTimer);
    clearInterval(flushTimer);

    if (draft.thinking && !draft.thinkingMs) {
      draft.thinkingMs = Date.now() - startedAt;
    }
    /*
     * A tool still in flight when the turn ended. Stopping is the reader's own doing, so it
     * is reported as stopped rather than failed; a dropped connection really is a failure.
     * The output is cleared either way -- there is no result to show, and the card falls
     * back to the status label.
     */
    draft.toolCalls = draft.toolCalls.map((call) =>
      call.status === 'running'
        ? {
            ...call,
            status: status === 'stopped' ? 'stopped' : 'error',
            output: '',
          }
        : call
    );
    if (status === 'done') {
      draft.sources = (script.sources ?? []).map((source) => ({
        id: generateUniqueId(),
        title: source.title,
        domain: source.domain,
        url: `https://${source.domain}${source.path}`,
      }));
      draft.followUps = script.followUps ?? [];
    }
    draft.status = status;
    draft.error = error;
    onFinish({ ...draft });
  };

  const apply = (event: StreamEvent) => {
    // Thinking ends at the first event that isn't thinking.
    if (event.kind !== 'thinking' && draft.thinking && !draft.thinkingMs) {
      draft.thinkingMs = Date.now() - startedAt;
    }

    switch (event.kind) {
      case 'thinking':
        draft.thinking += event.text;
        break;
      case 'toolStart':
        draft.toolCalls = [
          ...draft.toolCalls,
          {
            id: generateUniqueId(),
            name: event.name,
            input: event.input,
            output: '',
            status: 'running',
          },
        ];
        break;
      case 'toolEnd': {
        const last = draft.toolCalls.length - 1;
        draft.toolCalls = draft.toolCalls.map((call, index) =>
          index === last
            ? {
                ...call,
                output: event.output,
                status: event.failed ? 'error' : 'done',
              }
            : call
        );
        break;
      }
      case 'content':
        draft.content += event.text;
        break;
      case 'fail':
        finish('error', event.error);
        return;
    }
    dirty = true;
  };

  const step = () => {
    const event = events[cursor];
    if (!event) {
      finish('done');
      return;
    }
    cursor += 1;
    apply(event);
    if (finished) return;

    const next = events[cursor];
    eventTimer = setTimeout(
      step,
      next ? jitter(delayBefore(next)) : STREAM_FLUSH_MS
    );
  };

  eventTimer = setTimeout(step, FIRST_EVENT_MS);

  return { stop: () => finish('stopped') };
}
