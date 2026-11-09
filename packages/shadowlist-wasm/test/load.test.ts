/*
 * loadShadowListKit with the module handed over as source, the way a page that fetched
 * shadowlist-wasm/shadowlistkit.wasm itself loads it, and loads that fail. Run ./build.sh first.
 */
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { test } from 'node:test';

import {
  ShadowListKitCore,
  isShadowListKitLoaded,
  loadShadowListKit,
} from '../dist/ShadowListKitCore.js';

/*
 * A module importing x.y, which the core's imports do not have.
 */
const UNLINKABLE = new Uint8Array([
  0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 2, 7, 1, 1, 120, 1, 121, 0, 0,
]);

test('rejects bytes that are not a module', async () => {
  await assert.rejects(
    loadShadowListKit({ source: new Uint8Array([0, 1, 2, 3]) })
  );
  assert.equal(isShadowListKitLoaded(), false);
});

test('rejects a compiled module that does not instantiate', async () => {
  await assert.rejects(
    loadShadowListKit({ source: await WebAssembly.compile(UNLINKABLE) })
  );
  assert.equal(isShadowListKitLoaded(), false);
});

test('loads from a compiled module given as source after a failed load', async () => {
  const require = createRequire(import.meta.url);
  const bytes = await readFile(
    require.resolve('shadowlist-wasm/shadowlistkit.wasm')
  );
  await loadShadowListKit({ source: await WebAssembly.compile(bytes) });
  assert.equal(isShadowListKitLoaded(), true);
  const core = new ShadowListKitCore(() => 50);
  core.destroy();
});
