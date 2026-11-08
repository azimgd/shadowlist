/*
 * loadShadowListKit with the module handed over as source, the way a page that fetched
 * shadowlist-wasm/shadowlistkit.wasm itself loads it. Run ./build.sh first.
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

test('loads from a compiled module given as source', async () => {
  const require = createRequire(import.meta.url);
  const bytes = await readFile(
    require.resolve('shadowlist-wasm/shadowlistkit.wasm')
  );
  await loadShadowListKit({ source: await WebAssembly.compile(bytes) });
  assert.equal(isShadowListKitLoaded(), true);
  const core = new ShadowListKitCore(() => 50);
  core.destroy();
});
