import { generateId } from '../internal/generateId';
import type { AssistantStreamStore } from './stream';
import type {
  AssistantSource,
  AssistantToolCall,
  AssistantTurn,
} from './types';

// ~20 renders a second: the flush cadence common chat SDKs default to.
const DEFAULT_FLUSH_MS = 50;

export interface CreateTurnWriterOptions {
  store: AssistantStreamStore;
  messageId: string;
  flushMs?: number;
  // Receives the final turn, already written to the store; commit it to the list data here.
  onFinish?: (turn: AssistantTurn) => void;
}

export interface AssistantTurnWriter {
  readonly isFinished: boolean;
  appendThinking: (text: string) => void;
  appendContent: (text: string) => void;
  // Returns the call's id (`id` when given, otherwise a generated one).
  startToolCall: (call: { id?: string; name: string; input: string }) => string;
  completeToolCall: (id: string, output: string) => void;
  failToolCall: (id: string, output?: string) => void;
  complete: (result?: {
    sources?: readonly AssistantSource[];
    followUps?: readonly string[];
  }) => void;
  fail: (error?: string) => void;
  stop: () => void;
}

/*
 * Feeds one streaming turn into the store. Tokens mutate a private draft and a fixed-cadence
 * flush publishes a snapshot only when something changed, so renders are capped at
 * 1000 / flushMs a second however fast tokens arrive. Calls after the turn finished are
 * ignored: a token racing a stop is expected, not an error.
 */
export function createTurnWriter({
  store,
  messageId,
  flushMs = DEFAULT_FLUSH_MS,
  onFinish,
}: CreateTurnWriterOptions): AssistantTurnWriter {
  const startedAt = Date.now();
  let content = '';
  let thinking = '';
  let thinkingMs: number | undefined;
  // Replaced, never mutated, so untouched calls keep their identity for memo.
  let toolCalls: readonly AssistantToolCall[] = [];
  let dirty = false;
  let finished = false;

  store.set(messageId, { status: 'streaming', content });

  const snapshot = () => ({ content, thinking, thinkingMs, toolCalls });

  const flushTimer = setInterval(() => {
    if (!dirty) return;
    dirty = false;
    store.set(messageId, { status: 'streaming', ...snapshot() });
  }, flushMs);

  // Thinking ends at the first event that isn't thinking.
  const endThinking = () => {
    if (thinking && thinkingMs === undefined) {
      thinkingMs = Date.now() - startedAt;
    }
  };

  const updateToolCall = (
    id: string,
    update: (call: AssistantToolCall) => AssistantToolCall
  ) => {
    toolCalls = toolCalls.map((call) => (call.id === id ? update(call) : call));
  };

  const finish = (turn: AssistantTurn) => {
    finished = true;
    clearInterval(flushTimer);
    store.set(messageId, turn);
    onFinish?.(turn);
  };

  /*
   * A tool still in flight when the turn ended. Stopping is the reader's own doing, so it is
   * reported as stopped rather than failed; a dropped connection really is a failure.
   */
  const settleRunningCalls = (status: 'failed' | 'stopped') => {
    toolCalls = toolCalls.map((call) =>
      call.status === 'running'
        ? { id: call.id, name: call.name, input: call.input, status }
        : call
    );
  };

  return {
    get isFinished() {
      return finished;
    },
    appendThinking: (text) => {
      if (finished) return;
      thinking += text;
      dirty = true;
    },
    appendContent: (text) => {
      if (finished) return;
      endThinking();
      content += text;
      dirty = true;
    },
    startToolCall: ({ id = generateId(), name, input }) => {
      if (finished) return id;
      endThinking();
      toolCalls = [...toolCalls, { id, name, input, status: 'running' }];
      dirty = true;
      return id;
    },
    completeToolCall: (id, output) => {
      if (finished) return;
      endThinking();
      updateToolCall(id, (call) => ({
        id: call.id,
        name: call.name,
        input: call.input,
        status: 'done',
        output,
      }));
      dirty = true;
    },
    failToolCall: (id, output) => {
      if (finished) return;
      endThinking();
      updateToolCall(id, (call) => ({
        id: call.id,
        name: call.name,
        input: call.input,
        status: 'failed',
        output,
      }));
      dirty = true;
    },
    complete: (result) => {
      if (finished) return;
      endThinking();
      settleRunningCalls('failed');
      finish({
        status: 'done',
        ...snapshot(),
        sources: result?.sources,
        followUps: result?.followUps,
      });
    },
    fail: (error) => {
      if (finished) return;
      endThinking();
      settleRunningCalls('failed');
      finish({ status: 'failed', ...snapshot(), error });
    },
    stop: () => {
      if (finished) return;
      endThinking();
      settleRunningCalls('stopped');
      finish({ status: 'stopped', ...snapshot() });
    },
  };
}
