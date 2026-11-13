/*
 * Each fresh module has its own loader state, so successful source/locateFile modes can be
 * checked independently without exposing a reset method in the package API.
 */
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';

for (const name of ['Kit', 'Utils'] as const) {
  const stem = name === 'Kit' ? 'shadowlistkit' : 'shadowlistutils';
  const path =
    name === 'Kit'
      ? '../dist/ShadowListKitCore.js'
      : '../dist/utils/ShadowListUtilsCore.js';
  for (const mode of ['bytes', 'locateFile'] as const) {
    test(`${name} loader accepts ${mode} and shares one in-flight promise`, async () => {
      const module = await import(`${path}?${mode}`);
      const load = module[`loadShadowList${name}`] as (
        options: object
      ) => Promise<void>;
      const loaded = module[`isShadowList${name}Loaded`] as () => boolean;
      let requests = 0;
      const source = new URL(`../dist/${stem}.wasm`, import.meta.url);
      const options =
        mode === 'bytes'
          ? { source: await readFile(source) }
          : {
              locateFile: (file: string) => {
                requests++;
                assert.equal(file, `${stem}.wasm`);
                return source.pathname;
              },
            };
      assert.equal(loaded(), false);
      const first = load(options);
      assert.equal(load(options), first);
      await first;
      assert.equal(loaded(), true);
      assert.equal(requests, mode === 'locateFile' ? 1 : 0);
    });
  }
}
