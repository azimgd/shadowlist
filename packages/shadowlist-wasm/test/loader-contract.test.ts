/*
 * Each fresh module has its own loader state, so successful source/locateFile modes can be
 * checked independently without exposing a reset method in the package API.
 */
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';

for (const name of ['Kit'] as const) {
  const stem = 'shadowlistkit';
  const path = '../dist/ShadowListKitCore.js';
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

/*
 * Emscripten 7 reads no wasmBinary unless a build asks for it, and a module that ignored the
 * bytes would load its own file instead. Bytes reach the module through instantiateWasm.
 */
test('bytes instantiate through instantiateWasm, which every Emscripten reads', async () => {
  const { ShadowListWasmLoader } =
    await import('../dist/ShadowListWasmLoader.js');
  const bytes = await readFile(
    new URL('../dist/shadowlistkit.wasm', import.meta.url)
  );
  const seen: object[] = [];
  const loader = new ShadowListWasmLoader(
    (options: Record<string, unknown>) => {
      seen.push(options);
      return new Promise((resolve) => {
        const instantiate = options.instantiateWasm as (
          imports: WebAssembly.Imports,
          done: (instance: WebAssembly.Instance) => void
        ) => object;
        // A module with imports the core does not need fails to link, which is enough here.
        instantiate({}, () => resolve('linked'));
      });
    },
    () => {}
  );
  await assert.rejects(loader.load({ source: bytes }));
  assert.equal(seen.length, 1);
  assert.equal('wasmBinary' in seen[0]!, false);
  assert.equal(
    typeof (seen[0] as { instantiateWasm?: unknown }).instantiateWasm,
    'function'
  );
});
