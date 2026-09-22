import type { AssistantStreamStore } from '../stream';
import { createTurnWriter } from '../turnWriter';
import type { AssistantTurn } from '../types';

function fakeStore() {
  const turns = new Map<string, AssistantTurn>();
  const writes: AssistantTurn[] = [];
  const store: AssistantStreamStore = {
    subscribe: () => () => {},
    get: (id) => turns.get(id),
    set: (id, turn) => {
      turns.set(id, turn);
      writes.push(turn);
    },
    remove: (id) => {
      turns.delete(id);
    },
  };
  return { store, writes, turn: () => turns.get('m1') };
}

describe('createTurnWriter', () => {
  beforeEach(() => jest.useFakeTimers());
  afterEach(() => jest.useRealTimers());

  it('batches tokens into one write per flush', () => {
    const { store, writes, turn } = fakeStore();
    const writer = createTurnWriter({ store, messageId: 'm1', flushMs: 50 });
    expect(writes).toHaveLength(1);

    writer.appendContent('Hel');
    writer.appendContent('lo');
    jest.advanceTimersByTime(50);
    expect(writes).toHaveLength(2);
    expect(turn()?.content).toBe('Hello');

    // Nothing new, so nothing is written.
    jest.advanceTimersByTime(200);
    expect(writes).toHaveLength(2);
    writer.stop();
  });

  it('completes, stops flushing and ignores late tokens', () => {
    const { store, writes, turn } = fakeStore();
    const onFinish = jest.fn();
    const writer = createTurnWriter({ store, messageId: 'm1', onFinish });
    writer.appendContent('Done');
    writer.complete({ followUps: ['More?'] });

    expect(turn()?.status).toBe('done');
    expect(onFinish).toHaveBeenCalledTimes(1);
    expect(writer.isFinished).toBe(true);

    const count = writes.length;
    writer.appendContent(' late');
    writer.fail('late');
    jest.advanceTimersByTime(1000);
    expect(writes).toHaveLength(count);
    expect(turn()?.content).toBe('Done');
    expect(jest.getTimerCount()).toBe(0);
  });

  it('settles running tool calls as stopped on stop and failed on fail', () => {
    const stopped = fakeStore();
    const a = createTurnWriter({ store: stopped.store, messageId: 'm1' });
    a.startToolCall({ id: 't1', name: 'search', input: '{}' });
    a.stop();
    expect(stopped.turn()?.toolCalls?.[0]?.status).toBe('stopped');

    const failed = fakeStore();
    const b = createTurnWriter({ store: failed.store, messageId: 'm1' });
    const id = b.startToolCall({ name: 'search', input: '{}' });
    b.completeToolCall(id, 'ok');
    b.startToolCall({ id: 't2', name: 'fetch', input: '{}' });
    b.fail('network');
    const calls = failed.turn()?.toolCalls ?? [];
    expect(calls.map((call) => call.status)).toEqual(['done', 'failed']);
    expect(failed.turn()?.status).toBe('failed');
  });

  it('records where each call was made and starts a paragraph after one', () => {
    const { store, turn } = fakeStore();
    const writer = createTurnWriter({ store, messageId: 'm1' });
    writer.appendContent("I'll add the table.");
    const first = writer.startToolCall({ name: 'fill', input: '{}' });
    writer.completeToolCall(first, '40 cells');
    const second = writer.startToolCall({ name: 'read', input: '{}' });
    writer.appendContent(' Now the dates.');
    writer.stop();

    expect(turn()?.content).toBe("I'll add the table.\n\nNow the dates.");
    const calls = turn()?.toolCalls ?? [];
    expect(calls.map((call) => call.at)).toEqual([19, 19]);
    // Settling a running call keeps its place in the text.
    expect(calls.find((call) => call.id === second)?.status).toBe('stopped');
    expect(calls.find((call) => call.id === second)?.at).toBe(19);
  });

  it('rewinds the text a failed attempt streamed', () => {
    const { store, turn } = fakeStore();
    const writer = createTurnWriter({ store, messageId: 'm1' });
    writer.appendContent('Adding the column.');
    const start = writer.contentLength;
    writer.appendContent(' Now freezing the val');
    writer.rewindContent(start);
    writer.appendContent(' Now freezing the values.');
    writer.stop();
    expect(turn()?.content).toBe('Adding the column. Now freezing the values.');
  });
});
