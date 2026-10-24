/*
 * Checks every README in the repo: each local link and heading anchor resolves, and each table is
 * written compact, `| a | b |` with three-dash separators. This is what holds their layout because
 * Prettier leaves the READMEs alone (.prettierignore). It also checks the rows and lines a README
 * repeats from the build configuration (requirements and install versions), and compiles the
 * standalone examples.
 *
 *   node scripts/check-readmes.mjs
 *
 * `--fix` rewrites the tables compact before it checks.
 *
 * shadowlist, shadowgrid and ream each keep a copy of this script. Only the configuration block
 * differs between them. Change the rest in all three.
 */
import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const read = (file) => readFileSync(path.resolve(root, file), 'utf8');
const json = (file) => JSON.parse(read(file));

/*
 * The first group of `pattern` in `file`, or undefined. A miss is reported by the text it was
 * meant to fill.
 */
const capture = (file, pattern) => read(file).match(pattern)?.[1];

/* ---------- This repo's configuration ---------- */

/*
 * READMEs written by a tool rather than by hand.
 */
const SKIPPED = new Set([]);

/*
 * The tsconfig standalone `export function` examples compile with, or null for none.
 */
const EXAMPLES_TSCONFIG = 'templates/shadowlist-fabric-example/tsconfig.json';

/*
 * Each entry is a line a README must contain, built from the configuration it repeats.
 */
function requiredLines() {
  const fabric = json('packages/shadowlist-fabric/package.json');
  const utils = json('templates/shadowlist-utils/package.json');
  const kitBuild = 'packages/shadowlist-android/ShadowListKit/build.gradle.kts';
  const kitProperties =
    'packages/shadowlist-android/ShadowListKit/gradle.properties';
  const iosMinimum = capture(
    'Package.swift',
    /platforms: \[\.iOS\(\.v(\d+)\)\]/
  );
  const androidMinimum = capture(kitBuild, /minSdk = (\d+)/);
  const javaMinimum = capture(
    kitBuild,
    /sourceCompatibility = JavaVersion.VERSION_(\d+)/
  );
  const mavenVersion = capture(kitProperties, /^VERSION_NAME=(.+)$/m);
  const peers = (file) =>
    ['react', 'react-native'].map((name) => ({
      file,
      text: `| \`${name}\` | \`${fabric.peerDependencies[name]}\``,
      what: `${name} requirement (shadowlist's peer range)`,
    }));
  return [
    ...peers('packages/shadowlist-fabric/README.md'),
    ...peers('templates/shadowlist-utils/README.md'),
    {
      file: 'templates/shadowlist-utils/README.md',
      text: `| \`shadowlist\` | \`${utils.peerDependencies.shadowlist}\``,
      what: 'shadowlist peer range',
    },
    {
      file: 'packages/shadowlist-uikit/README.md',
      text: iosMinimum && `| iOS | ${iosMinimum} or newer |`,
      what: 'iOS requirement (Package.swift)',
    },
    {
      file: 'packages/shadowlist-uikit/README.md',
      text: `from: "${fabric.version}"`,
      what: 'SwiftPM install version',
    },
    {
      file: 'packages/shadowlist-android/README.md',
      text: androidMinimum && `| Android | API ${androidMinimum} or newer |`,
      what: `Android requirement (${kitBuild})`,
    },
    {
      file: 'packages/shadowlist-android/README.md',
      text: javaMinimum && `| Java | ${javaMinimum} or newer |`,
      what: `Java requirement (${kitBuild})`,
    },
    {
      file: 'packages/shadowlist-android/README.md',
      text: mavenVersion && `io.github.azimgd:shadowlist-kit:${mavenVersion}`,
      what: `Maven install version (${kitProperties})`,
    },
  ];
}

/* ---------- The same in every repo ---------- */

const fix = process.argv.includes('--fix');
const files = execFileSync(
  'git',
  ['ls-files', '--cached', '--others', '--exclude-standard', '-z'],
  { cwd: root, encoding: 'utf8' }
)
  .split('\0')
  .filter((file) => /(^|\/)readme\.md$/i.test(file) && !SKIPPED.has(file));
const errors = [];
const FENCE = /^```[^\n]*\n[\s\S]*?^```\s*$/gm;
const withoutCode = (source) => source.replace(FENCE, '');

/*
 * The cells of a table row, trimmed, with a separator cell written as three dashes.
 */
function cellsOf(line) {
  return line
    .split(/(?<!\\)\|/)
    .slice(1, -1)
    .map((cell) => {
      const value = cell.trim();
      return /^:?-{3,}:?$/.test(value) ? value.replace(/-+/g, '---') : value;
    });
}

const compact = (line) => `| ${cellsOf(line).join(' | ')} |`;

/*
 * The source with every table row outside a code block written compact.
 */
function compacted(source) {
  let isCode = false;
  return source
    .split('\n')
    .map((line) => {
      if (line.startsWith('```')) isCode = !isCode;
      return !isCode && line.startsWith('|') ? compact(line) : line;
    })
    .join('\n');
}

function anchors(source) {
  const result = new Set();
  const counts = new Map();
  for (const match of withoutCode(source).matchAll(/^#{1,6} (.+)$/gm)) {
    const slug = match[1]
      .toLowerCase()
      .replace(/[^\p{L}\p{M}\p{N}_\- ]/gu, '')
      .replace(/ /g, '-');
    const count = counts.get(slug) ?? 0;
    result.add(count ? `${slug}-${count}` : slug);
    counts.set(slug, count + 1);
  }
  return result;
}

const documents = new Map();
for (const file of files) {
  let source = read(file);
  if (fix) {
    const fixed = compacted(source);
    if (fixed !== source) writeFileSync(path.resolve(root, file), fixed);
    source = fixed;
  }
  documents.set(file, source);
}

for (const [file, source] of documents) {
  const prose = withoutCode(source);
  for (const match of prose.matchAll(/\[[^\]]*\]\(([^\s)]+)\)/g)) {
    const href = match[1];
    if (/^[a-z][a-z\d+.-]*:/i.test(href)) continue;
    const [target, fragment] = href.split('#');
    const resolved = target
      ? path.resolve(root, path.dirname(file), decodeURIComponent(target))
      : path.resolve(root, file);
    if (!existsSync(resolved)) {
      errors.push(`${file}: missing link target ${href}`);
    } else if (
      fragment &&
      /\.md$/i.test(resolved) &&
      !anchors(readFileSync(resolved, 'utf8')).has(decodeURIComponent(fragment))
    ) {
      errors.push(`${file}: missing heading ${href}`);
    }
  }
  if (
    prose
      .split('\n')
      .some((line) => line.startsWith('|') && line !== compact(line))
  ) {
    errors.push(`${file}: use compact table cells and three-dash separators`);
  }
}

const required = requiredLines();
for (const { file, text, what } of required) {
  if (!text) {
    errors.push(`${file}: ${what} could not be read from the build files`);
  } else if (!documents.get(file)?.includes(text)) {
    errors.push(`${file}: ${what} must read "${text}"`);
  }
}

/*
 * Standalone exported examples compile in memory with the example tsconfig's dependency
 * resolution. Short JSX fragments read the app's variables. Their complete linked screens run in
 * yarn typecheck.
 */
const examples = new Map();
if (EXAMPLES_TSCONFIG) {
  const { default: ts } = await import('typescript');
  for (const [file, source] of documents) {
    for (const match of source.matchAll(/^```tsx\n([\s\S]*?)^```/gm)) {
      if (!/^export function /m.test(match[1])) continue;
      const virtualPath = path.join(
        root,
        path.dirname(EXAMPLES_TSCONFIG),
        `readme-example-${examples.size}.tsx`
      );
      const line = source.slice(0, match.index).split('\n').length;
      examples.set(virtualPath, { source: match[1], label: `${file}:${line}` });
    }
  }
  const config = ts.getParsedCommandLineOfConfigFile(
    path.join(root, EXAMPLES_TSCONFIG),
    { noEmit: true },
    {
      ...ts.sys,
      onUnRecoverableConfigFileDiagnostic: (diagnostic) =>
        errors.push(
          ts.flattenDiagnosticMessageText(diagnostic.messageText, '\n')
        ),
    }
  );
  if (config) {
    const host = ts.createCompilerHost(config.options);
    const originalRead = host.readFile;
    const originalExists = host.fileExists;
    const originalSource = host.getSourceFile;
    host.readFile = (file) => examples.get(file)?.source ?? originalRead(file);
    host.fileExists = (file) => examples.has(file) || originalExists(file);
    host.getSourceFile = (file, languageVersion, ...args) =>
      examples.has(file)
        ? ts.createSourceFile(
            file,
            examples.get(file).source,
            languageVersion,
            true,
            ts.ScriptKind.TSX
          )
        : originalSource(file, languageVersion, ...args);
    const program = ts.createProgram(
      [...examples.keys()],
      config.options,
      host
    );
    for (const diagnostic of [
      ...config.errors,
      ...ts.getPreEmitDiagnostics(program),
    ]) {
      const label =
        examples.get(diagnostic.file?.fileName)?.label ??
        diagnostic.file?.fileName ??
        'TypeScript';
      const location =
        diagnostic.file && diagnostic.start !== undefined
          ? diagnostic.file.getLineAndCharacterOfPosition(diagnostic.start)
          : undefined;
      errors.push(
        `${label}${location ? ` (example line ${location.line + 1})` : ''}: ${ts.flattenDiagnosticMessageText(diagnostic.messageText, '\n')}`
      );
    }
  }
}

if (errors.length) {
  console.error(errors.join('\n'));
  process.exitCode = 1;
} else {
  console.log(
    `Checked ${files.length} READMEs: local links, headings, compact tables, ${required.length} required lines and ${examples.size} standalone examples.`
  );
}
