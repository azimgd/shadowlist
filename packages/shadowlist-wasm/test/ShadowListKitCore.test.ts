/*
 * The wasm core through ShadowListKitCore, without a browser. Run ./build.sh first.
 */
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { before, test } from 'node:test';
import { runInNewContext } from 'node:vm';
import { ModuleKind, transpileModule } from 'typescript';
import type { ShadowListKitScrollGesture } from '../src/ShadowListKitScrollGesture.ts';

import {
  ShadowListKitCore,
  ShadowListKitListState,
  loadShadowListKit,
  keySplice,
  easeOut,
} from '../dist/ShadowListKitCore.js';

const ROW_SIZE = 50;
const WINDOW = 500;

/*
 * Run the gesture source with a manual browser clock. No browser, rebuilt dist, or real
 * timer is needed to check how input events and display frames change the gesture.
 */
function scrollGesture(firesScrollend = true) {
  const source = readFileSync(
    new URL('../src/ShadowListKitScrollGesture.ts', import.meta.url),
    'utf8'
  );
  const code = transpileModule(source, {
    compilerOptions: { module: ModuleKind.CommonJS },
  }).outputText;
  let now = 1000;
  let nextId = 0;
  const timers = new Map<number, { at: number; run: () => void }>();
  const frames = new Map<number, (time: number) => void>();
  const listeners = new Map<string, (event: object) => void>();
  const events: unknown[] = [];
  const list = {
    offset: 25,
    movedElsewhere: false,
    snapTarget: (offset: number) => offset,
    addEventListener: (name: string, run: (event: object) => void) =>
      listeners.set(name, run),
    removeEventListener: (name: string) => listeners.delete(name),
    gestureBegan: () => events.push('gesture'),
    draggingBegan: () => events.push('drag'),
    draggingEnded: (decelerates: boolean) =>
      events.push(['release', decelerates]),
    scrollingEnded: (command: boolean) => events.push(['end', command]),
    scrollStep: (offset: number) => {
      list.offset = offset;
    },
  };
  const exports = {} as {
    ShadowListKitScrollGesture: new (
      list: object
    ) => ShadowListKitScrollGesture;
  };
  runInNewContext(code, {
    exports,
    require: () => ({ ShadowListKitCore, easeOut }),
    window: firesScrollend ? { onscrollend: null } : {},
    performance: { now: () => now },
    setTimeout: (run: () => void, delay: number) => {
      const id = ++nextId;
      timers.set(id, { at: now + delay, run });
      return id;
    },
    clearTimeout: (id: number) => timers.delete(id),
    requestAnimationFrame: (run: (time: number) => void) => {
      const id = ++nextId;
      frames.set(id, run);
      return id;
    },
    cancelAnimationFrame: (id: number) => frames.delete(id),
  });
  const gesture = new exports.ShadowListKitScrollGesture(list);
  gesture.attach();
  return {
    gesture,
    list,
    events,
    timers,
    frames,
    listeners,
    event(name: string, event: object = {}) {
      listeners.get(name)?.(event);
    },
    advance(milliseconds: number) {
      const until = now + milliseconds;
      for (;;) {
        const next = [...timers].sort((a, b) => a[1].at - b[1].at)[0];
        if (!next || next[1].at > until) break;
        now = next[1].at;
        timers.delete(next[0]);
        next[1].run();
      }
      now = until;
    },
    frame(milliseconds: number) {
      now += milliseconds;
      const work = [...frames.values()];
      frames.clear();
      for (const run of work) run(now);
    },
  };
}

test('gesture holds an ended touch until the last finger lifts', () => {
  const h = scrollGesture();
  h.event('touchstart');
  h.gesture.userScrolled();
  assert.equal(h.gesture.isTracking, true);
  assert.equal(h.gesture.isSettling, true);
  assert.equal(h.timers.size, 0);
  h.event('scrollend');
  h.advance(0);
  assert.deepEqual(h.events, ['gesture', 'drag']);
  h.event('touchend', { touches: [{}] });
  assert.equal(h.gesture.isTracking, true);
  h.event('touchend', { touches: [] });
  assert.deepEqual(h.events, [
    'gesture',
    'drag',
    ['release', true],
    ['end', false],
  ]);
  assert.equal(h.gesture.isTracking, false);
  assert.equal(h.gesture.isSettling, false);
  assert.equal(h.timers.size, 0);
});

test('gesture waits for a new scrollend if movement follows an end while held', () => {
  const h = scrollGesture();
  h.event('touchstart');
  h.gesture.userScrolled();
  h.event('scrollend');
  h.gesture.userScrolled();
  h.event('touchend', { touches: [] });
  assert.equal(h.timers.size, 0);
  assert.equal(h.gesture.isSettling, true);
  h.event('scrollend');
  assert.equal(h.gesture.isSettling, true);
  h.advance(0);
  assert.equal(h.gesture.isSettling, false);
  assert.deepEqual(h.events.at(-1), ['end', false]);
});

test('gesture restarts the wheel quiet wait and ignores intermediate scrollend', () => {
  const h = scrollGesture();
  const quiet = ShadowListKitCore.WHEEL_IDLE_MS;
  h.event('wheel');
  assert.equal(h.gesture.isSettling, false);
  h.gesture.userScrolled();
  h.event('scrollend');
  h.advance(quiet - 1);
  assert.equal(h.gesture.isSettling, true);
  h.event('wheel');
  h.gesture.userScrolled();
  h.advance(quiet - 1);
  assert.equal(h.gesture.isSettling, true);
  h.advance(1);
  assert.equal(h.gesture.isSettling, false);
  assert.equal(h.timers.size, 0);
  assert.deepEqual(h.events.at(-1), ['end', false]);
  h.gesture.userScrolled();
  assert.equal(h.timers.size, 0);
  h.event('scrollend');
  h.advance(0);
  assert.equal(h.gesture.isSettling, false);
});

test('gesture quiet fallback cannot finish a held scroll and restarts on release', () => {
  const h = scrollGesture(false);
  h.event('touchstart');
  h.gesture.userScrolled();
  h.advance(ShadowListKitCore.WHEEL_IDLE_MS);
  assert.equal(h.gesture.isSettling, true);
  assert.equal(h.gesture.isTracking, true);
  assert.equal(h.timers.size, 0);
  h.event('touchcancel', { touches: [] });
  assert.equal(h.gesture.isTracking, false);
  assert.equal(h.timers.size, 1);
  h.advance(ShadowListKitCore.WHEEL_IDLE_MS);
  assert.equal(h.gesture.isSettling, false);
  assert.deepEqual(h.events.at(-1), ['end', false]);
});

test('gesture scrollbar tracking replaces wheel completion and ignores row presses', () => {
  const h = scrollGesture();
  h.event('pointerdown', { pointerType: 'mouse', target: {} });
  assert.equal(h.gesture.isTracking, false);
  h.event('wheel');
  h.gesture.userScrolled();
  h.event('pointerdown', { pointerType: 'mouse', target: h.list });
  h.advance(ShadowListKitCore.WHEEL_IDLE_MS);
  assert.equal(h.gesture.isTracking, true);
  h.event('pointercancel', { pointerType: 'mouse' });
  assert.equal(h.gesture.isSettling, true);
  assert.equal(h.timers.size, 0);
  h.event('scrollend');
  h.advance(0);
  assert.equal(h.gesture.isSettling, false);
});

test('gesture animation replaces quiet work and touch takeover cancels its frame', () => {
  const h = scrollGesture();
  h.event('wheel');
  h.gesture.userScrolled();
  h.gesture.animateTo(() => 100);
  assert.equal(h.timers.size, 0);
  assert.equal(h.gesture.isSettling, false);
  assert.equal(h.gesture.isAnimating, true);
  h.event('scrollend');
  assert.equal(h.timers.size, 0);
  h.event('touchstart');
  assert.equal(h.frames.size, 0);
  assert.equal(h.gesture.isAnimating, false);
  assert.equal(h.gesture.isTracking, true);
  h.event('touchend', { touches: [] });
  assert.deepEqual(h.events.at(-2), ['release', false]);
  assert.deepEqual(h.events.at(-1), ['end', false]);
});

test('gesture completes commands while held and follows corrected live targets', () => {
  const h = scrollGesture();
  h.event('touchstart');
  let target = 100;
  h.gesture.animateTo(() => target);
  assert.equal(h.gesture.isTracking, true);
  assert.equal(h.gesture.isAnimating, true);
  h.gesture.offsetCorrected(10);
  h.frame(0);
  assert.equal(h.list.offset, 35);
  target = 150;
  h.frame(ShadowListKitCore.COMMAND_DURATION_MS);
  assert.equal(h.list.offset, 150);
  assert.equal(h.gesture.isAnimating, false);
  assert.equal(h.gesture.isTracking, true);
  assert.deepEqual(h.events.at(-1), ['end', true]);
});

test('gesture detach cancels completion and frames and clears its readable phases', () => {
  for (const animate of [false, true]) {
    const h = scrollGesture();
    h.event('touchstart');
    h.event('wheel');
    h.gesture.userScrolled();
    if (animate) h.gesture.animateTo(() => 100);
    h.gesture.detach();
    assert.equal(h.gesture.isTracking, false);
    assert.equal(h.gesture.isSettling, false);
    assert.equal(h.gesture.isAnimating, false);
    assert.equal(h.timers.size, 0);
    assert.equal(h.frames.size, 0);
    assert.equal(h.listeners.size, 0);
  }
});

before(() => loadShadowListKit());

function keys(count: number, prefix = 'row'): string[] {
  return Array.from({ length: count }, (_, index) => `${prefix}-${index}`);
}

/*
 * The keys a state holds become next, with the change reaching core when given. Returns the
 * packed result, see ShadowListKitListState.commitKeys.
 */
function commit(
  state: ShadowListKitListState,
  core: ShadowListKitCore | null,
  previous: string[],
  next: string[],
  change: Parameters<ShadowListKitListState['commitKeys']>[4] = {}
): Int32Array {
  return state.commitKeys(
    core,
    previous,
    next,
    keySplice(previous, next),
    change
  )!;
}

/*
 * A packed commit result as its parts.
 */
function changesOf(packed: Int32Array) {
  const values = Array.from(packed);
  let at = 0;
  const take = (count: number) => values.slice(at, (at += count));
  const deleted = take(take(1)[0]!);
  const inserted = take(take(1)[0]!);
  const moved = take(take(1)[0]! * 2);
  const reloaded = take(take(1)[0]!);
  return { deleted, inserted, moved, reloaded };
}

/*
 * A core with count rows of ROW_SIZE and one layout at offset, its keys held by a state.
 */
function laidOutCore(
  count: number,
  offset = 0,
  settings: {
    estimatedRowSize?: number;
    numberOfColumns?: number;
    inverted?: boolean;
    snapToItem?: boolean;
  } = {}
) {
  const measured: number[] = [];
  const core = new ShadowListKitCore((row) => {
    measured.push(row);
    return ROW_SIZE;
  });
  core.setSettings(
    settings.estimatedRowSize ?? 100,
    1,
    1,
    1,
    settings.numberOfColumns ?? 1,
    settings.inverted ?? false,
    false,
    false,
    settings.snapToItem ?? false,
    0
  );
  const rows = keys(count);
  const state = new ShadowListKitListState();
  commit(state, core, [], rows);
  runPasses(core, offset);
  return { core, measured, rows, state };
}

function runPasses(core: ShadowListKitCore, offset: number): Float64Array {
  core.pass[ShadowListKitCore.PASS_OFFSET] = offset;
  core.pass[ShadowListKitCore.PASS_WINDOW_ALONG] = WINDOW;
  core.pass[ShadowListKitCore.PASS_WINDOW_CROSS] = 320;
  core.pass[ShadowListKitCore.PASS_PHASE] = ShadowListKitCore.PHASE_IDLE;
  core.runPasses();
  return core.pass;
}

test('key splice finds the changed middle as the core does', () => {
  assert.deepEqual(keySplice(['c', 'd'], ['a', 'b', 'c', 'd']), {
    start: 0,
    deleted: 0,
    inserted: 2,
  });
  assert.deepEqual(keySplice(['a', 'b', 'c', 'd'], ['a', 'b']), {
    start: 2,
    deleted: 2,
    inserted: 0,
  });
  assert.deepEqual(keySplice(['a', 'b', 'c', 'd'], ['a', 'x', 'y', 'd']), {
    start: 1,
    deleted: 2,
    inserted: 2,
  });
  assert.deepEqual(keySplice(['a', 'a'], ['a']), {
    start: 1,
    deleted: 1,
    inserted: 0,
  });
  assert.deepEqual(keySplice([], []), { start: 0, deleted: 0, inserted: 0 });
});

test('a layout measures the rows in range and reports the content size', () => {
  const { core, measured } = laidOutCore(100);
  const pass = core.pass;
  assert.equal(core.rowCount, 100);
  assert.ok(measured.includes(0));
  assert.ok(measured.length < 100);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_OFFSET], 0);
  assert.ok(pass[ShadowListKitCore.PASS_OUT_CONTENT] > WINDOW);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_MEASURED_LOW], 0);
  assert.ok(
    pass[ShadowListKitCore.PASS_OUT_MEASURED_HIGH] >= WINDOW / ROW_SIZE
  );
  core.destroy();
});

test('row rects follow the measured sizes', () => {
  const { core } = laidOutCore(100);
  const out = new Float64Array(8);
  core.copyRowRects(0, 1, out);
  assert.deepEqual(Array.from(out), [
    0,
    0,
    320,
    ROW_SIZE,
    0,
    ROW_SIZE,
    320,
    ROW_SIZE,
  ]);
  const single = new Float64Array(4);
  assert.equal(core.rowRect(3, single), true);
  assert.equal(single[1], 3 * ROW_SIZE);
  assert.equal(core.rowRect(1000, single), false);
  core.destroy();
});

test('a prepend keeps the visible row in place', () => {
  const { core, rows, state } = laidOutCore(100, 1000);
  const anchor = core.anchorState(1000);
  assert.equal(anchor?.key, rows[15]);
  commit(state, core, rows, [...keys(10, 'new'), ...rows]);
  const pass = runPasses(core, 1000);
  const offset = pass[ShadowListKitCore.PASS_OUT_OFFSET];
  assert.equal(core.anchorState(offset!)?.key, rows[15]);
  assert.equal(core.indexOfKey(rows[15]), 25);
  core.destroy();
});

test('an inverted list opens at its end', () => {
  const { core } = laidOutCore(100, 0, { inverted: true });
  const pass = runPasses(core, core.pass[ShadowListKitCore.PASS_OUT_OFFSET]);
  const content = pass[ShadowListKitCore.PASS_OUT_CONTENT];
  assert.equal(pass[ShadowListKitCore.PASS_OUT_OFFSET], content - WINDOW);
  assert.equal(core.visibleRange()?.high, 99);
  core.destroy();
});

test('scroll to row lands the row at the viewport start', () => {
  const { core } = laidOutCore(100);
  core.scrollToRow(40, 0);
  const pass = runPasses(core, 0);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_OFFSET], 40 * ROW_SIZE);
  core.destroy();
});

test('keys survive non ascii text', () => {
  const { core, rows, state } = laidOutCore(3);
  commit(state, core, rows, [rows[0]!, 'ключ-🙂', rows[2]!]);
  runPasses(core, 0);
  assert.equal(core.indexOfKey('ключ-🙂'), 1);
  assert.equal(core.restoreAnchorState('ключ-🙂', 0), true);
  core.destroy();
});

test('insert and delete keys change the count', () => {
  const { core, rows, state } = laidOutCore(10);
  state.insertRows(core, rows, [0, 5], ['a', 'b']);
  state.deleteRows(core, rows, [2]);
  runPasses(core, 0);
  assert.equal(core.rowCount, 11);
  assert.equal(core.indexOfKey('a'), 0);
  assert.equal(core.indexOfKey('b'), 4);
  core.destroy();
});

/*
 * The diff of previous and next through a state that holds previous.
 */
function diffOf(previous: string[], next: string[]): number[] {
  const state = new ShadowListKitListState();
  commit(state, null, [], previous);
  return Array.from(commit(state, null, previous, next, { diff: true }));
}

test('diff keys reports deletes inserts and moves', () => {
  const diff = diffOf(['a', 'b', 'c', 'd'], ['b', 'a', 'd', 'e']);
  // One delete (c at 2), one insert (e at 3), one move.
  assert.deepEqual(diff.slice(0, 2), [1, 2]);
  assert.deepEqual(diff.slice(2, 4), [1, 3]);
  assert.equal(diff[4], 1);
});

test('diff keys reports indices in the whole lists past unchanged ends', () => {
  assert.deepEqual(
    diffOf(['p', 'q', 'a', 'b', 'c'], ['p', 'q', 'a', 'c']),
    [1, 3, 0, 0, 0]
  );
  assert.deepEqual(
    diffOf(['x', 'a', 'b', 'y'], ['x', 'b', 'a', 'n', 'y']).slice(0, 4),
    [0, 1, 3, 1]
  );
  assert.deepEqual(diffOf(['a', 'b'], ['a', 'b']), [0, 0, 0, 0]);
});

test('a list state collects a batch until the outermost block and plans it', () => {
  const state = new ShadowListKitListState();
  assert.equal(state.isBatching, false);
  assert.equal(state.takeBatch(), null);
  state.beginBatch();
  state.batchDeleted([1]);
  state.beginBatch();
  state.batchInserted([0, -3]);
  state.batchReloaded([2]);
  assert.equal(state.endBatch(), false);
  assert.equal(state.isBatching, true);
  assert.equal(state.endBatch(), true);
  assert.deepEqual(Array.from(state.takeBatch()!), [0, 2]);
  assert.deepEqual(Array.from(state.planTakenBatch(3, 3)!), [-1, 0, 2]);
  // A taken batch plans once, and a batch that does not add up plans to null.
  state.beginBatch();
  state.batchInserted([0]);
  state.endBatch();
  state.takeBatch();
  assert.equal(state.planTakenBatch(3, 5), null);
  state.beginBatch();
  state.batchNeedsReload();
  state.endBatch();
  assert.deepEqual(Array.from(state.takeBatch()!), [1]);
});

test('a list state reloads items whose number or string version changed', () => {
  const state = new ShadowListKitListState();
  const first = ['a', 'b', 'c', 'd', 'e'];
  commit(state, null, [], first, { record: [1, 'v1', -1, 0.5, 2 ** 40] });
  const second = ['e', 'd', 'c', 'b', 'a', 'new'];
  assert.deepEqual(
    changesOf(
      commit(state, null, first, second, {
        update: [2 ** 40 + 1, 0.5, -1, 'v2', '1', 7],
      })
    ).reloaded,
    [0, 3, 4]
  );
  // The new versions are kept, and the same versions again change nothing.
  assert.deepEqual(
    changesOf(commit(state, null, second, ['b', 'new'], { update: ['v2', 7] }))
      .reloaded,
    []
  );
});

test('insertion and deletion indices are sorted and clipped', () => {
  assert.deepEqual(
    Array.from(ShadowListKitCore.insertionIndices([9, 1, 1], 3)),
    [1, 4]
  );
  assert.deepEqual(
    Array.from(ShadowListKitCore.deletionIndices([5, 2, 0, 2], 3)),
    [0, 2]
  );
});

test('a measure that calls into the core keeps the pass slots', () => {
  let core: ShadowListKitCore;
  core = new ShadowListKitCore(() => {
    core!.indexOfKey('x'.repeat(100000));
    return ROW_SIZE;
  });
  core.setSettings(100, 1, 1, 1, 1, false, false, false, false, 0);
  const rows = keys(50);
  commit(new ShadowListKitListState(), core, [], rows);
  const pass = runPasses(core, 0);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_MEASURED_LOW], 0);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_CONTENT], 50 * ROW_SIZE);
  core.destroy();
});

test('the visible range is the rows on screen, not the measured range', () => {
  const { core } = laidOutCore(100, 1000);
  /*
   * The first pass measures the rows at the offset. Rows 0 to 4 keep the estimate of 100 above
   * them, and rows 15 to 24 fill the viewport at 1000.
   */
  runPasses(core, 1000);
  assert.deepEqual(core.visibleRange(), { low: 15, high: 24 });
  assert.ok(core.pass[ShadowListKitCore.PASS_OUT_MEASURED_LOW]! < 15);
  core.destroy();
});

test('a reset keeps how far the viewport is into its first row', () => {
  const { core, rows } = laidOutCore(100, 1020);
  runPasses(core, 1020);
  core.resetKeepingPosition();
  const pass = runPasses(core, 1020);
  const offset = pass[ShadowListKitCore.PASS_OUT_OFFSET]!;
  assert.deepEqual(core.anchorState(offset), { key: rows[15], offset: 20 });
  core.destroy();
});

test('a reset keeps an inverted list at its end', () => {
  const { core } = laidOutCore(100, 0, { inverted: true });
  let pass = runPasses(core, core.pass[ShadowListKitCore.PASS_OUT_OFFSET]!);
  const end = pass[ShadowListKitCore.PASS_OUT_CONTENT]! - WINDOW;
  core.resetKeepingPosition();
  pass = runPasses(core, end);
  assert.equal(
    pass[ShadowListKitCore.PASS_OUT_OFFSET],
    pass[ShadowListKitCore.PASS_OUT_CONTENT]! - WINDOW
  );
  core.destroy();
});

test('a list state seeds a new core with the keys it holds', () => {
  const { core, rows, state } = laidOutCore(20);
  core.destroy();
  const next = new ShadowListKitCore(() => ROW_SIZE);
  next.setSettings(100, 1, 1, 1, 1, false, false, false, false, 0);
  state.seedCore(next, rows);
  runPasses(next, 0);
  assert.equal(next.rowCount, 20);
  assert.equal(next.indexOfKey(rows[19]!), 19);
  next.destroy();
});

test('runPasses with prefetch on reports rows before it measures them', () => {
  const events: string[] = [];
  let core: ShadowListKitCore;
  core = new ShadowListKitCore(
    (row) => {
      events.push(`m${row}`);
      return ROW_SIZE;
    },
    undefined,
    () => {
      for (const row of core!.prefetched ?? []) events.push(`p${row}`);
    }
  );
  core.setSettings(100, 1, 1, 1, 1, false, false, false, false, 0);
  const rows = keys(100);
  const state = new ShadowListKitListState();
  commit(state, core, [], rows);
  runPasses(core, 0);
  events.length = 0;
  core.pass[ShadowListKitCore.PASS_PREFETCH] = 1;
  runPasses(core, 200);
  const prefetched = events.filter((event) => event.startsWith('p'));
  assert.ok(prefetched.length > 0);
  for (const event of prefetched) {
    const measured = events.indexOf(`m${event.slice(1)}`);
    assert.ok(measured < 0 || events.indexOf(event) < measured);
  }
  core.destroy();
});

/*
 * The members of an enum in the core's host/KitBridge.hpp, in order: each is its position.
 */
function bridgeSlots(name: string): string[] {
  const header = readFileSync(
    new URL('../../shadowlist-core/host/KitBridge.hpp', import.meta.url),
    'utf8'
  );
  const body = new RegExp(`enum ${name} : int \\{([^}]+)\\}`).exec(header);
  assert.ok(body, `${name} is in KitBridge.hpp`);
  return body[1]!
    .split(',')
    .map((member) => member.split('=')[0]!.trim())
    .filter((member) => member.length > 0);
}

test("the pass, mount and sticky slots are the core's here and on Android", () => {
  const kotlin = readFileSync(
    new URL(
      '../../shadowlist-android/ShadowListKit/src/main/java/com/shadowlist/kit/ShadowListKitCore.kt',
      import.meta.url
    ),
    'utf8'
  );
  const mirrored = ShadowListKitCore as unknown as Record<string, number>;
  for (const name of ['PassSlot', 'MountSlot', 'StickySlot']) {
    const slots = bridgeSlots(name);
    assert.ok(slots.length > 8, name);
    slots.forEach((slot, position) => {
      assert.equal(mirrored[slot], position, `${slot} in ShadowListKitCore.ts`);
      const constant = new RegExp(`const val ${slot} = (\\d+)\\b`).exec(kotlin);
      assert.equal(
        Number(constant?.[1]),
        position,
        `${slot} in ShadowListKitCore.kt`
      );
    });
  }
});

test('mount records keep unsigned versions and decode invalid slots as unset', () => {
  const previous = new Float64Array(ShadowListKitCore.MOUNT_SLOTS);
  const next = new Float64Array(previous);
  previous[ShadowListKitCore.MOUNT_GEOMETRY] = 2 ** 63;
  next[ShadowListKitCore.MOUNT_GEOMETRY] = 2 ** 63;
  assert.equal(
    ShadowListKitCore.shouldSkipMount(previous, next, 0, 1, false),
    true
  );
  next[ShadowListKitCore.MOUNT_GEOMETRY] = 2 ** 63 + 2048;
  assert.equal(
    ShadowListKitCore.shouldSkipMount(previous, next, 0, 1, false),
    false
  );
  for (const slot of [
    ShadowListKitCore.MOUNT_GEOMETRY,
    ShadowListKitCore.MOUNT_STRUCTURE,
    ShadowListKitCore.MOUNT_LOW,
    ShadowListKitCore.MOUNT_HIGH,
    ShadowListKitCore.MOUNT_STICKY,
  ]) {
    previous.fill(0);
    next.fill(0);
    previous[slot] = -1;
    for (const invalid of [-0.5, 0.5, NaN, Infinity, -Infinity, 2 ** 64]) {
      next[slot] = invalid;
      assert.equal(
        ShadowListKitCore.shouldSkipMount(previous, next, 0, 1, false),
        true,
        `slot ${slot} with ${invalid}`
      );
    }
  }
});

test('a pass says whether an inverted list is at its end', () => {
  const { core } = laidOutCore(100, 0, { inverted: true });
  let pass = runPasses(core, core.pass[ShadowListKitCore.PASS_OUT_OFFSET]!);
  const end = pass[ShadowListKitCore.PASS_OUT_CONTENT]! - WINDOW;
  assert.equal(pass[ShadowListKitCore.PASS_OUT_AT_END], 1);
  // The reader scrolls away from the newest row.
  core.pass[ShadowListKitCore.PASS_USER_SCROLLED] = 1;
  core.pass[ShadowListKitCore.PASS_OFFSET] = end - 400;
  core.pass[ShadowListKitCore.PASS_PHASE] = ShadowListKitCore.PHASE_DRAGGING;
  core.runPasses();
  core.pass[ShadowListKitCore.PASS_USER_SCROLLED] = 0;
  pass = runPasses(core, core.pass[ShadowListKitCore.PASS_OUT_OFFSET]!);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_AT_END], 0);
  core.scrollToEnd();
  pass = runPasses(core, pass[ShadowListKitCore.PASS_OUT_OFFSET]!);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_AT_END], 1);
  core.destroy();
});

test('a list without rows counts its empty view with the header', () => {
  const { core, rows, state } = laidOutCore(0);
  core.pass[ShadowListKitCore.PASS_HEADER_SIZE] = 30;
  core.pass[ShadowListKitCore.PASS_FOOTER_SIZE] = 20;
  core.pass[ShadowListKitCore.PASS_EMPTY_SIZE] = 200;
  let pass = runPasses(core, 0);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_SHOWS_EMPTY], 1);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_FOOTER_START], 230);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_CONTENT], 250);
  commit(state, core, rows, keys(2));
  pass = runPasses(core, 0);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_SHOWS_EMPTY], 0);
  assert.equal(
    pass[ShadowListKitCore.PASS_OUT_FOOTER_START],
    30 + 2 * ROW_SIZE
  );
  core.destroy();
});

test('a non anchor key never holds the visible rows', () => {
  for (const nonAnchor of [false, true]) {
    const sizes = new Map<number, number>();
    const core = new ShadowListKitCore((row) => sizes.get(row) ?? ROW_SIZE);
    core.setSettings(100, 1, 1, 1, 1, false, false, false, false, 0);
    const state = new ShadowListKitListState();
    commit(state, core, [], keys(100));
    if (nonAnchor) core.setNonAnchorKeys(['row-10']);
    runPasses(core, 0);
    runPasses(core, 10 * ROW_SIZE);
    // The row at the viewport start grows by 40.
    sizes.set(10, ROW_SIZE + 40);
    core.markRemeasure([10]);
    const pass = runPasses(core, 10 * ROW_SIZE);
    assert.equal(
      pass[ShadowListKitCore.PASS_OUT_OFFSET],
      10 * ROW_SIZE + (nonAnchor ? 40 : 0)
    );
    core.destroy();
  }
});

test('the sticky layout pins the header and the footer inside their spans', () => {
  const slots = new Float64Array(ShadowListKitCore.STICKY_SLOTS);
  slots[ShadowListKitCore.STICKY_OFFSET] = 300;
  slots[ShadowListKitCore.STICKY_WINDOW_SIZE] = 600;
  slots[ShadowListKitCore.STICKY_CONTENT_SIZE] = 2000;
  slots[ShadowListKitCore.STICKY_HEADER_SIZE] = 50;
  slots[ShadowListKitCore.STICKY_FOOTER_SIZE] = 40;
  slots[ShadowListKitCore.STICKY_FOOTER_START] = 1960;
  slots[ShadowListKitCore.STICKY_HEADER] = 1;
  slots[ShadowListKitCore.STICKY_FOOTER] = 1;
  ShadowListKitCore.stickyLayout(slots);
  assert.equal(slots[ShadowListKitCore.STICKY_OUT_HEADER_TRANSLATION], 300);
  assert.equal(
    slots[ShadowListKitCore.STICKY_OUT_FOOTER_TRANSLATION],
    300 + 600 - 40 - 1960
  );
  assert.equal(slots[ShadowListKitCore.STICKY_OUT_HEADER_SPAN_START], 0);
  assert.equal(slots[ShadowListKitCore.STICKY_OUT_HEADER_SPAN_END], 1960);
  assert.equal(slots[ShadowListKitCore.STICKY_OUT_FOOTER_SPAN_START], 50);
  assert.equal(slots[ShadowListKitCore.STICKY_OUT_FOOTER_SPAN_END], 2000);
  assert.equal(slots[ShadowListKitCore.STICKY_OFFSET], 300);
});
