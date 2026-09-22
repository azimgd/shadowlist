#!/usr/bin/env node

/**
 * Extract variable bindings from every package source file and emit a TSV
 * inventory. JavaScript/TypeScript is parsed with the TypeScript compiler API;
 * native sources use a conservative declaration/parameter scanner so the
 * inventory remains useful without requiring a platform compiler.
 */

import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import ts from 'typescript';

const repositoryRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const packageRoot = resolve(repositoryRoot, 'packages');
const outputPath = resolve(repositoryRoot, 'reports/variable-names.tsv');
const sourceExtensions = [
  '*.js',
  '*.jsx',
  '*.ts',
  '*.tsx',
  '*.cpp',
  '*.hpp',
  '*.cc',
  '*.h',
  '*.mm',
  '*.m',
  '*.swift',
  '*.kt',
  '*.java',
];

/*
 * Tracked plus untracked-but-not-ignored files, so the gitignored vendored core
 * mirror and build output stay out of the inventory.
 */
const files = execFileSync(
  'git',
  [
    'ls-files',
    '--cached',
    '--others',
    '--exclude-standard',
    '--',
    ...sourceExtensions.map((pattern) => `packages/**/${pattern}`),
  ],
  { cwd: repositoryRoot, encoding: 'utf8' }
)
  .trim()
  .split('\n')
  .filter(Boolean)
  .map((file) => resolve(repositoryRoot, file))
  .filter((file) => existsSync(file))
  .sort();

const rows = [];

function lineNumber(sourceFile, position) {
  return sourceFile.getLineAndCharacterOfPosition(position).line + 1;
}

function bindingNames(node) {
  if (!node) return [];
  if (ts.isIdentifier(node)) return [node];
  if (ts.isBindingElement(node)) return bindingNames(node.name);
  if (ts.isObjectBindingPattern(node) || ts.isArrayBindingPattern(node)) {
    return node.elements.flatMap((element) =>
      ts.isOmittedExpression(element)
        ? []
        : bindingNames(element.name ?? element)
    );
  }
  return [];
}

function isConstant(name) {
  return /^[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*$/.test(name);
}

function toLowerCamel(name) {
  const words = name
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .replace(/([A-Z]+)([A-Z][a-z])/g, '$1 $2')
    .split(/[^A-Za-z0-9]+/)
    .filter(Boolean);
  if (words.length === 0) return name;
  const [first, ...rest] = words;
  return (
    first.toLowerCase() +
    rest
      .map((word) => word[0].toUpperCase() + word.slice(1).toLowerCase())
      .join('')
  );
}

/**
 * One spelling per concept (see docs/variable-naming-standard.md). Keys are
 * camelCase words that must not appear in an identifier; values are the word
 * to use instead. Word matching is case-insensitive and keeps the casing of
 * the original word.
 */
const VOCABULARY = {
  prev: 'previous',
  cur: 'current',
  curr: 'current',
  idx: 'index',
  cnt: 'count',
  len: 'length',
  pos: 'position',
  tmp: 'temp',
  dip: 'dp',
  btn: 'button',
  cfg: 'config',
  evt: 'event',
  msg: 'message',
};

// Public React Native spellings kept as-is (they are props, not local choices).
const PUBLIC_SPELLINGS = new Set(['testID', 'nativeID']);

function applyVocabulary(name) {
  return name.replace(
    /(^|[^A-Za-z])([a-z]+)|([A-Z][a-z]+)/g,
    (match, prefix, lower, capitalized) => {
      const word = lower ?? capitalized;
      const replacement = VOCABULARY[word.toLowerCase()];
      if (!replacement) return match;
      const cased = capitalized
        ? replacement[0].toUpperCase() + replacement.slice(1)
        : replacement;
      return (prefix ?? '') + cased;
    }
  );
}

function standardize(name, kind, language) {
  if (isConstant(name) && (kind === 'constant' || language === 'native'))
    return name;
  if (name === '_') return name;
  // Macro-local names carry a namespace prefix so they cannot capture caller names.
  if (kind === 'macro-local') return name;
  if (language === 'native' && (/^_/.test(name) || /_$/.test(name)))
    return name;
  if (language === 'javascript') {
    if (PUBLIC_SPELLINGS.has(name)) return name;
    // A leading underscore marks an intentionally unused binding.
    if (/^_[a-z]/.test(name)) return name;
    /*
     * Component-valued bindings (constants, or destructured props such as
     * ListHeaderComponent) stay PascalCase so JSX can render them.
     */
    if (
      (kind === 'constant' || kind === 'parameter') &&
      /^[A-Z][A-Za-z0-9]*$/.test(name)
    )
      return name;
  }
  return applyVocabulary(toLowerCamel(name));
}

function addRow(file, language, kind, name, line) {
  rows.push({
    package: relative(packageRoot, file).split('/')[0],
    file: relative(repositoryRoot, file),
    language,
    kind,
    name,
    standardizedName: standardize(name, kind, language),
    status:
      name === standardize(name, kind, language)
        ? 'standard'
        : 'needs-standardization',
    line,
  });
}

function extractJavaScript(file) {
  const source = readFileSync(file, 'utf8');
  const sourceFile = ts.createSourceFile(
    file,
    source,
    ts.ScriptTarget.Latest,
    true
  );

  function visit(node) {
    if (ts.isVariableDeclaration(node)) {
      const kind =
        ts.isVariableDeclarationList(node.parent) &&
        (node.parent.flags & ts.NodeFlags.Const) !== 0
          ? 'constant'
          : 'variable';
      for (const binding of bindingNames(node.name)) {
        addRow(
          file,
          'javascript',
          kind,
          binding.text,
          lineNumber(sourceFile, binding.pos)
        );
      }
    } else if (ts.isParameter(node)) {
      for (const binding of bindingNames(node.name)) {
        addRow(
          file,
          'javascript',
          'parameter',
          binding.text,
          lineNumber(sourceFile, binding.pos)
        );
      }
    } else if (ts.isCatchClause(node)) {
      for (const binding of bindingNames(node.variableDeclaration?.name)) {
        addRow(
          file,
          'javascript',
          'catch-binding',
          binding.text,
          lineNumber(sourceFile, binding.pos)
        );
      }
    }
    ts.forEachChild(node, visit);
  }

  visit(sourceFile);
}

function stripNativeComments(source) {
  return source
    .replace(/\/\*[\s\S]*?\*\//g, (comment) => comment.replace(/[^\n]/g, ' '))
    .replace(/\/\/.*$/gm, '');
}

function extractNative(file) {
  const source = stripNativeComments(readFileSync(file, 'utf8'));
  const lines = source.split('\n');
  const declaration =
    /\b(?:(?:const|constexpr|static\s+const)\s+)?(?:int|long|short|float|double|bool|auto|size_t|std::size_t|NSInteger|NSUInteger|CGFloat|BOOL|NSString\s*\*|id|uint(?:8|16|32|64)_t|int(?:8|16|32|64)_t|std::(?:string|unique_ptr|shared_ptr|optional)<[^;=]+>)\s+([A-Za-z_][A-Za-z0-9_]*)\b(?!\s*(?:::|\())/g;

  const isSwift = file.endsWith('.swift');
  let inMacro = false;

  lines.forEach((line, index) => {
    const macroLine = inMacro || /^\s*#\s*define\b/.test(line);
    inMacro = macroLine && /\\\s*$/.test(line);

    for (const match of line.matchAll(declaration)) {
      const name = match[1];
      const kind = macroLine
        ? 'macro-local'
        : isConstant(name)
          ? 'constant'
          : 'variable';
      addRow(file, 'native', kind, name, index + 1);
    }

    const looksLikeSignature =
      /\b[A-Za-z_][A-Za-z0-9_:<>*&\[\], ]*\s+[A-Za-z_][A-Za-z0-9_]*\s*\(/.test(
        line
      ) && !/\b(?:if|for|while|switch|static_cast)\s*\(/.test(line);
    const parameterList = looksLikeSignature
      ? line.match(/\(([^()]*)\)/)?.[1]
      : undefined;
    if (!parameterList) return;
    for (const parameter of parameterList.split(',')) {
      if (isSwift) {
        // Swift: `label name: Type` or `name: Type`; the binding is the word before the colon.
        const swiftMatch = parameter
          .trim()
          .match(/([A-Za-z_][A-Za-z0-9_]*)\s*:/);
        if (swiftMatch)
          addRow(file, 'native', 'parameter', swiftMatch[1], index + 1);
        continue;
      }
      const match = parameter
        .trim()
        .match(
          /(?:const\s+)?[\w:<>*&\[\], ]+\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:=|$)/
        );
      if (match) addRow(file, 'native', 'parameter', match[1], index + 1);
    }
  });
}

for (const file of files) {
  if (/\.(js|jsx|ts|tsx)$/.test(file)) extractJavaScript(file);
  else extractNative(file);
}

rows.sort(
  (left, right) =>
    left.file.localeCompare(right.file) ||
    left.line - right.line ||
    left.name.localeCompare(right.name)
);

mkdirSync(dirname(outputPath), { recursive: true });
const header = [
  'package',
  'file',
  'language',
  'kind',
  'name',
  'standardizedName',
  'status',
  'line',
];
const escape = (value) =>
  String(value).replace(/\t/g, ' ').replace(/\r?\n/g, ' ');
writeFileSync(
  outputPath,
  [header, ...rows.map((row) => header.map((column) => escape(row[column])))]
    .map((row) => row.join('\t'))
    .join('\n') + '\n'
);

const counts = rows.reduce((result, row) => {
  result[row.language] = (result[row.language] ?? 0) + 1;
  return result;
}, {});
const nonStandard = rows.filter(
  (row) => row.status === 'needs-standardization'
).length;
console.log(
  `Extracted ${rows.length} variable names (${counts.javascript ?? 0} JavaScript/TypeScript, ${counts.native ?? 0} native).`
);
console.log(`${nonStandard} names are flagged for standardization.`);
console.log(`Wrote ${relative(repositoryRoot, outputPath)}.`);

if (rows.length < 2000) {
  console.error(
    'Variable inventory is below the required minimum of 2,000 names.'
  );
  process.exitCode = 1;
}
