/*
 * Mirror the canonical shared core (packages/shadowlist-core) into this package
 * so the published npm tarball is self-contained - the package.json `files`
 * field ships `shadowlist-core/`, and a consumer's pod install / gradle build
 * compiles it from inside node_modules/shadowlist where `../shadowlist-core` is
 * not reachable.
 *
 * Runs from the `prepare` lifecycle (local install + publish). It is a no-op
 * when the canonical source is absent (e.g. already inside a consumer install),
 * leaving any existing vendored copy untouched. The destination is wiped first
 * so it is always an exact mirror: a per-file copy would leave behind sources
 * that were since removed from the canonical core.
 *
 * The vendored copy is gitignored - it is a generated artifact, not source.
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
