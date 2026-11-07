import assert from 'node:assert/strict';
import { test } from 'node:test';
import {
  PHASE_IDLE,
  ShadowListKitCore,
  shadowListStoreModule,
} from '../src/ShadowListKitCore';

/*
 * The addon alone, no platform: it loads, the store keeps versions and a selection, and a driver
 * lays out a large list while measuring few rows.
 */

const SETTINGS = {
  overscan: 1,
  startReachedThreshold: 1,
  endReachedThreshold: 1,
  numberOfColumns: 1,
  inverted: false,
  followAppends: false,
  horizontal: false,
  snapToItem: false,
  snapAlignment: 0,
};

function keysOf(count: number): string[] {
  return Array.from({ length: count }, (_, index) => `k${index}`);
}

test('the store keeps versions of keys and answers lookups', () => {
  const store = shadowListStoreModule();
  const handle = store.createStore();
  assert.ok(handle.id > 0);
  assert.equal(store.setKeys(handle, 1, ['a', 'b', 'c']), true);
  assert.equal(store.indexOfKey(handle, 1, 'c', -1), 2);
  assert.equal(store.indexOfKey(handle, 1, 'z', -1), -1);
  assert.equal(store.spliceKeys(handle, 2, 1, 1, 1, ['x', 'y']), true);
  assert.equal(store.indexOfKey(handle, 2, 'y', -1), 2);
  assert.equal(store.indexOfKey(handle, 9, 'a', -1), -2);
  assert.equal(store.spliceKeys(handle, 3, 7, 0, 0, []), false);
});

test('the store keeps a selection by the core rules', () => {
  const store = shadowListStoreModule();
  const handle = store.createStore();
  store.setKeys(handle, 1, ['a', 'b', 'c']);
  assert.deepEqual(store.selectKey(handle, 'a'), ['a']);
  assert.deepEqual(store.selectKey(handle, 'b'), ['b']);
  assert.equal(store.selectKey(handle, 'b'), null);
  store.setSelection(handle, ['a', 'c'], true);
  assert.deepEqual(store.selectedIndices(handle, 1), [0, 2]);
  assert.deepEqual(store.selectOnlyKey(handle, 'b'), ['b']);
  assert.deepEqual(store.deselectKey(handle, 'b'), []);
  store.setSelection(handle, ['a', 'c'], true);
  store.setKeys(handle, 2, ['c']);
  assert.deepEqual(store.retainSelection(handle, 2), ['c']);
});

test('a driver adopts 100,000 keys and measures only the rows near the window', () => {
  const store = shadowListStoreModule();
  const handle = store.createStore();
  store.setKeys(handle, 1, keysOf(100000));
  const core = new ShadowListKitCore();
  core.setSettings(SETTINGS);
  assert.equal(core.adoptKeys(handle.id, 1), true);
  assert.equal(core.adoptKeys(handle.id, 2), false);
  let measures = 0;
  const input = {
    offset: 0,
    windowAlong: 20,
    windowCross: 80,
    headerSize: 0,
    footerSize: 0,
    phase: PHASE_IDLE,
    userScrolled: false,
  };
  const result = core.runPasses(input, () => {
    measures++;
    return 1;
  });
  assert.equal(result.content, 100000);
  assert.ok(measures < 100, `measured ${measures}`);
  assert.deepEqual(core.getVisibleRange(), { low: 0, high: 19 });
  assert.deepEqual(core.getRowRect(7), { x: 0, y: 7, width: 80, height: 1 });

  core.scrollToRow(50000, 0, 0);
  const landed = core.runPasses(input, () => 1);
  assert.equal(landed.offset, 50000);
  assert.deepEqual(core.getAnchorState(50000), { key: 'k50000', offset: 0 });
});

test('a measure callback that throws reaches the caller', () => {
  const store = shadowListStoreModule();
  const handle = store.createStore();
  store.setKeys(handle, 1, keysOf(3));
  const core = new ShadowListKitCore();
  core.setSettings(SETTINGS);
  core.adoptKeys(handle.id, 1);
  assert.throws(
    () =>
      core.runPasses(
        {
          offset: 0,
          windowAlong: 20,
          windowCross: 80,
          headerSize: 0,
          footerSize: 0,
          phase: PHASE_IDLE,
          userScrolled: false,
        },
        () => {
          throw new Error('measure failed');
        }
      ),
    /measure failed/
  );
});
