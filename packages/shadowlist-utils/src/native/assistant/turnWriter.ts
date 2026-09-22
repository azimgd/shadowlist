import { generateId } from '../internal/generateId';
import type { AssistantStreamStore } from './stream';
import type {
  AssistantSource,
  AssistantToolCall,
  AssistantTurn,
} from './types';

// About 20 renders a second, the usual default in chat kits.
const DEFAULT_FLUSH_MS = 50;

export interface CreateTurnWriterOptions {
  store: AssistantStreamStore;
  messageId: string;
  flushMs?: number;
  // Gets the final turn, already in the store. Commit it to the list data here.
  onFinish?: (turn: AssistantTurn) => void;
}

export interface AssistantTurnWriter {
  readonly isFinished: boolean;
  // How much text the reply holds so far: where a request that may be retried started writing.
  readonly contentLength: number;
  appendThinking: (text: string) => void;
  appendContent: (text: string) => void;
  /*
   * Takes the text back to `length`. A request that fails partway -- the connection drops, the
   * server resets a long stream -- has already streamed some of its answer, and the retry
   * streams it again from the start; leaving the first attempt's words on screen printed the same
   * sentence two or three times over, run together.
   */
  rewindContent: (length: number) => void;
  // Returns the given id, or a generated one.
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
 * Feeds one streaming turn into the store. Tokens update a private draft, and a timer
 * publishes it only when something changed, so renders stay capped however fast tokens
 * come. Calls after the turn ends are ignored, since a token can race a stop.
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
  // Replaced, never mutated, so untouched calls keep their identity.
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

  /*
   * Thinking ends at the first event that isn't thinking.
   */
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

  /*
   * Fields every call state keeps, its identity and where in the text it happened.
   */
  const base = (call: AssistantToolCall) => ({
    id: call.id,
    name: call.name,
    input: call.input,
    ...(call.at === undefined ? {} : { at: call.at }),
  });

  /*
   * Text after a tool call starts a new paragraph. Models narrate in pieces with nothing
   * between them, so otherwise two sentences run together.
   */
  let afterCall = false;

  const finish = (turn: AssistantTurn) => {
    finished = true;
    clearInterval(flushTimer);
    store.set(messageId, turn);
    onFinish?.(turn);
  };

  /*
   * Settles tools still running when the turn ended. A stop by the reader marks them
   * stopped, a dropped connection marks them failed.
   */
  const settleRunningCalls = (status: 'failed' | 'stopped') => {
    toolCalls = toolCalls.map((call) =>
      call.status === 'running' ? { ...base(call), status } : call
    );
  };

  return {
    get isFinished() {
      return finished;
    },
    get contentLength() {
      return content.length;
    },
    rewindContent: (length) => {
      if (finished || length >= content.length) return;
      content = content.slice(0, Math.max(0, length));
      toolCalls = toolCalls.map((call) =>
        call.at !== undefined && call.at > content.length
          ? { ...call, at: content.length }
          : call
      );
      dirty = true;
    },
    appendThinking: (text) => {
      if (finished) return;
      thinking += text;
      dirty = true;
    },
    appendContent: (text) => {
      if (finished) return;
      endThinking();
      if (afterCall && text.trim() !== '') {
        afterCall = false;
        if (content.trim() !== '' && !content.endsWith('\n\n')) {
          content += content.endsWith('\n') ? '\n' : '\n\n';
        }
        text = text.replace(/^\s+/, '');
      }
      content += text;
      dirty = true;
    },
    startToolCall: ({ id = generateId(), name, input }) => {
      if (finished) return id;
      endThinking();
      afterCall = true;
      toolCalls = [
        ...toolCalls,
        { id, name, input, at: content.length, status: 'running' },
      ];
      dirty = true;
      return id;
    },
    completeToolCall: (id, output) => {
      if (finished) return;
      endThinking();
      updateToolCall(id, (call) => ({ ...base(call), status: 'done', output }));
      dirty = true;
    },
    failToolCall: (id, output) => {
      if (finished) return;
      endThinking();
      updateToolCall(id, (call) => ({
        ...base(call),
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
