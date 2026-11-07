/*
 * Runs the tests the way a consumer runs the package: esbuild bundles each test with `react-native`
 * aliased to react-native-tty, from the sibling ream checkout through this package's `link:`
 * dependency, and one `react` for all of them. Nothing else in this repo resolves `react-native`
 * this way, so its jest and its typecheck see the real one. Build the addon first (`yarn build`).
 *
 *   node test/run.mjs [node --test arguments]
 */
import { spawnSync } from 'node:child_process';
import { readdirSync, realpathSync, rmSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { build } from 'esbuild';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.dirname(here);
const out = path.join(root, 'build/test');
const platform = path.dirname(
  realpathSync(path.join(root, 'node_modules/react-native-tty/package.json'))
);

const tests = readdirSync(here).filter((file) => /\.test\.tsx?$/.test(file));
rmSync(out, { recursive: true, force: true });
await build({
  entryPoints: tests.map((file) => path.join(here, file)),
  outdir: out,
  outExtension: { '.js': '.cjs' },
  bundle: true,
  platform: 'node',
  format: 'cjs',
  target: 'node22',
  sourcemap: 'inline',
  resolveExtensions: ['.tty.tsx', '.tty.ts', '.tsx', '.ts', '.mjs', '.js'],
  tsconfigRaw: { compilerOptions: { jsx: 'react-jsx' } },
  define: {
    '__DEV__': 'false',
    'process.env.NODE_ENV': '"production"',
  },
  alias: {
    'react-native': path.join(platform, 'src/index.ts'),
    'react': path.dirname(
      createRequire(path.join(platform, 'src/index.ts')).resolve(
        'react/package.json'
      )
    ),
  },
  logOverride: { 'empty-import-meta': 'silent' },
  logLevel: 'warning',
});

const result = spawnSync(
  process.execPath,
  [
    '--test',
    ...process.argv.slice(2),
    ...tests.map((file) => path.join(out, file.replace(/\.tsx?$/, '.cjs'))),
  ],
  {
    stdio: 'inherit',
    env: {
      ...process.env,
      SHADOWLIST_TTY_ADDON: path.join(root, 'build/shadowlist_tty.node'),
    },
  }
);
process.exitCode = result.status ?? 1;
