/*
 * Builds cpp/ into build/shadowlist_tty.node against the headers of the Node running this.
 *
 *   node scripts/build-addon.mjs [Release|Debug]
 */
import { execFileSync } from 'node:child_process';
import path from 'node:path';

const root = path.resolve(path.dirname(process.argv[1]), '..');
const config = process.argv[2] || 'Release';
const cmakeDir = path.join(root, 'build', 'cmake', config);
const nodeInclude = path.resolve(
  path.dirname(process.execPath),
  '..',
  'include',
  'node'
);
const run = (args) =>
  execFileSync('cmake', args, { cwd: root, stdio: 'inherit' });

run([
  '-S',
  'cpp',
  '-B',
  cmakeDir,
  `-DCMAKE_BUILD_TYPE=${config}`,
  `-DNODE_INCLUDE_DIR=${nodeInclude}`,
  '-DCMAKE_RULE_MESSAGES=OFF',
]);
run(['--build', cmakeDir, '-j', '8']);
