/*
 * Copy packages/shadowlist-core into this package so the npm tarball builds on
 * its own. Inside a user's node_modules the sibling core folder isn't there.
 *
 * Runs on prepare. Does nothing when the core folder is missing, which keeps any
 * existing copy. Wipes the copy first so removed files don't linger.
 * The copy is gitignored.
 */
const fs = require('fs');
const path = require('path');

const srcDir = path.join(__dirname, '..', '..', 'shadowlist-core');
const destDir = path.join(__dirname, '..', 'shadowlist-core');

if (!fs.existsSync(srcDir)) {
  console.log(
    '[vendor-core] canonical ../shadowlist-core not found; keeping any existing copy'
  );
  process.exit(0);
}

fs.rmSync(destDir, { recursive: true, force: true });
fs.cpSync(srcDir, destDir, {
  recursive: true,
  filter: (src) => fs.statSync(src).isDirectory() || /\.(cpp|hpp)$/.test(src),
});

console.log(
  '[vendor-core] mirrored shadowlist-core ->',
  path.relative(process.cwd(), destDir)
);
