import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import ts from 'typescript';

const root = fileURLToPath(new URL('../', import.meta.url));
const read = (file) => readFileSync(path.resolve(root, file), 'utf8');
const files = execFileSync(
  'git',
  ['ls-files', '--cached', '--others', '--exclude-standard', '-z'],
  { cwd: root, encoding: 'utf8' }
)
  .split('\0')
  .filter((file) => /(^|\/)readme\.md$/i.test(file));
const documents = new Map(files.map((file) => [file, read(file)]));
const errors = [];
const withoutCode = (source) =>
  source.replace(/^```[^\n]*\n[\s\S]*?^```\s*$/gm, '');

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
    } else if (fragment && /\.md$/i.test(resolved)) {
      if (
        !anchors(readFileSync(resolved, 'utf8')).has(
          decodeURIComponent(fragment)
        )
      ) {
        errors.push(`${file}: missing heading ${href}`);
      }
    }
  }
  for (const line of prose.split('\n')) {
    if (!line.startsWith('|')) continue;
    const cells = line
      .split(/(?<!\\)\|/)
      .slice(1, -1)
      .map((cell) => {
        const value = cell.trim();
        return /^:?-{3,}:?$/.test(value) ? value.replace(/-+/g, '---') : value;
      });
    if (line !== `| ${cells.join(' | ')} |`) {
      errors.push(`${file}: use compact table cells and three-dash separators`);
      break;
    }
  }
}

const fabric = JSON.parse(read('packages/shadowlist-fabric/package.json'));
const utils = JSON.parse(read('templates/shadowlist-utils/package.json'));
for (const file of [
  'packages/shadowlist-fabric/README.md',
  'templates/shadowlist-utils/README.md',
]) {
  for (const name of ['react', 'react-native']) {
    const requirement = fabric.peerDependencies[name];
    if (!documents.get(file).includes(`| \`${name}\` | \`${requirement}\``)) {
      errors.push(
        `${file}: ${name} requirement must match shadowlist (${requirement})`
      );
    }
  }
}
if (
  !documents
    .get('templates/shadowlist-utils/README.md')
    .includes(`| \`shadowlist\` | \`${utils.peerDependencies.shadowlist}\``)
) {
  errors.push(
    'templates/shadowlist-utils/README.md: shadowlist peer range is stale'
  );
}

const iosMinimum = read('Package.swift').match(
  /platforms: \[\.iOS\(\.v(\d+)\)\]/
)?.[1];
const androidBuild = read(
  'packages/shadowlist-android/ShadowListKit/build.gradle.kts'
);
const androidMinimum = androidBuild.match(/minSdk = (\d+)/)?.[1];
const javaMinimum = androidBuild.match(
  /sourceCompatibility = JavaVersion.VERSION_(\d+)/
)?.[1];
for (const [file, text, value] of [
  ['packages/shadowlist-uikit/README.md', '| iOS |', `${iosMinimum} or newer`],
  [
    'packages/shadowlist-android/README.md',
    '| Android |',
    `API ${androidMinimum} or newer`,
  ],
  [
    'packages/shadowlist-android/README.md',
    '| Java |',
    `${javaMinimum} or newer`,
  ],
]) {
  if (!documents.get(file).includes(`${text} ${value} |`)) {
    errors.push(
      `${file}: requirement must match build configuration (${text} ${value})`
    );
  }
}

const androidVersion = read(
  'packages/shadowlist-android/ShadowListKit/gradle.properties'
).match(/^VERSION_NAME=(.+)$/m)?.[1];
if (
  !documents
    .get('packages/shadowlist-android/README.md')
    .includes(`io.github.azimgd:shadowlist-kit:${androidVersion}`)
) {
  errors.push(
    'packages/shadowlist-android/README.md: Maven install version is stale'
  );
}
if (
  !documents
    .get('packages/shadowlist-uikit/README.md')
    .includes(`from: "${fabric.version}"`)
) {
  errors.push(
    'packages/shadowlist-uikit/README.md: SwiftPM install version is stale'
  );
}

// Compile standalone exported examples in memory using the example app's dependency resolution.
// Short JSX fragments rely on application variables; their complete linked screens run in yarn typecheck.
const examples = new Map();
for (const [file, source] of documents) {
  for (const match of source.matchAll(/^```tsx\n([\s\S]*?)^```/gm)) {
    if (!/^export function /m.test(match[1])) continue;
    const virtualPath = path.join(
      root,
      'templates/shadowlist-fabric-example',
      `readme-example-${examples.size}.tsx`
    );
    const line = source.slice(0, match.index).split('\n').length;
    examples.set(virtualPath, { source: match[1], label: `${file}:${line}` });
  }
}
const config = ts.getParsedCommandLineOfConfigFile(
  path.join(root, 'templates/shadowlist-fabric-example/tsconfig.json'),
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
  const program = ts.createProgram([...examples.keys()], config.options, host);
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

if (errors.length) {
  console.error(errors.join('\n'));
  process.exitCode = 1;
} else {
  console.log(
    `Checked ${files.length} READMEs: local links, headings, compact tables, requirements and ${examples.size} standalone examples.`
  );
}
