/*
 * The wasm core through ShadowListKitCore, without a browser. Run ./build.sh first.
 */
import assert from 'node:assert/strict';
import { before, test } from 'node:test';

import {
  ShadowListKitCore,
  loadShadowListKit,
} from '../dist/ShadowListKitCore.js';

const ROW_SIZE = 50;
const WINDOW = 500;

before(() => loadShadowListKit());

function keys(count: number, prefix = 'row'): string[] {
  return Array.from({ length: count }, (_, index) => `${prefix}-${index}`);
}

/*
 * A core with count rows of ROW_SIZE and one layout at offset.
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
  core.replaceKeys(0, 0, rows, 0, rows.length);
  runPasses(core, offset);
  return { core, measured, rows };
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
  const { core, rows } = laidOutCore(100, 1000);
  const anchor = core.anchorState(1000);
  assert.equal(anchor?.key, rows[15]);
  core.replaceKeys(0, 0, keys(10, 'new'), 0, 10);
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
  const { core } = laidOutCore(3);
  core.replaceKeys(1, 1, ['ключ-🙂'], 0, 1);
  runPasses(core, 0);
  assert.equal(core.indexOfKey('ключ-🙂'), 1);
  assert.equal(core.restoreAnchorState('ключ-🙂', 0), true);
  core.destroy();
});

test('insert and delete keys change the count', () => {
  const { core } = laidOutCore(10);
  core.insertKeys([0, 5], ['a', 'b']);
  core.deleteKeys([2]);
  runPasses(core, 0);
  assert.equal(core.rowCount, 11);
  assert.equal(core.indexOfKey('a'), 0);
  assert.equal(core.indexOfKey('b'), 4);
  core.destroy();
});

test('diff keys reports deletes inserts and moves', () => {
  const diff = Array.from(
    ShadowListKitCore.diffKeys(['a', 'b', 'c', 'd'], ['b', 'a', 'd', 'e'])
  );
  // One delete (c at 2), one insert (e at 3), one move.
  assert.deepEqual(diff.slice(0, 2), [1, 2]);
  assert.deepEqual(diff.slice(2, 4), [1, 3]);
  assert.equal(diff[4], 1);
});

test('diff keys reports indices in the whole lists past unchanged ends', () => {
  assert.deepEqual(
    Array.from(
      ShadowListKitCore.diffKeys(
        ['p', 'q', 'a', 'b', 'c'],
        ['p', 'q', 'a', 'c']
      )
    ),
    [1, 3, 0, 0]
  );
  assert.deepEqual(
    Array.from(
      ShadowListKitCore.diffKeys(
        ['x', 'a', 'b', 'y'],
        ['x', 'b', 'a', 'n', 'y']
      )
    ).slice(0, 4),
    [0, 1, 3, 1]
  );
  assert.deepEqual(
    Array.from(ShadowListKitCore.diffKeys(['a', 'b'], ['a', 'b'])),
    [0, 0, 0]
  );
});

test('plan batch keeps rows and marks inserts', () => {
  assert.deepEqual(
    Array.from(ShadowListKitCore.planBatch(3, 3, [1], [0], [], [])!),
    [-1, 0, 2]
  );
  assert.equal(ShadowListKitCore.planBatch(3, 5, [], [0], [], []), null);
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
  core.replaceKeys(0, 0, rows, 0, rows.length);
  const pass = runPasses(core, 0);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_MEASURED_LOW], 0);
  assert.equal(pass[ShadowListKitCore.PASS_OUT_CONTENT], 50 * ROW_SIZE);
  core.destroy();
});

test('the visible range is the rows on screen, not the measured range', () => {
  const { core } = laidOutCore(100, 1000);
  // The first pass measures the rows at the offset. Rows 0 to 4 keep the estimate of 100 above
  // them, so rows 15 to 24 fill the viewport at 1000.
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
