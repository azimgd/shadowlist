/*
 * The wasm core through ShadowListKitCore, without a browser. Run ./build.sh first.
 */
import assert from 'node:assert/strict';
import { before, test } from 'node:test';

import {
  ShadowListKitCore,
  ShadowListKitListState,
  loadShadowListKit,
  unchangedEnds,
} from '../dist/ShadowListKitCore.js';

const ROW_SIZE = 50;
const WINDOW = 500;

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
    unchangedEnds(previous, next),
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
