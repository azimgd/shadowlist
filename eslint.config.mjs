import { fixupConfigRules } from '@eslint/compat';
import { FlatCompat } from '@eslint/eslintrc';
import js from '@eslint/js';
import prettier from 'eslint-plugin-prettier';
import { defineConfig } from 'eslint/config';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const compat = new FlatCompat({
  baseDirectory: __dirname,
  recommendedConfig: js.configs.recommended,
  allConfig: js.configs.all,
});

/*
 * Imports in the order docs/code-style.md gives: react, react-native, other packages, then
 * relative paths. A side effect import keeps its place. The repo has no eslint-plugin-import,
 * and this one rule is all it would be used for.
 */
function importRank(source) {
  if (source === 'react' || source.startsWith('react/')) {
    return 0;
  }
  if (source === 'react-native' || source.startsWith('react-native/')) {
    return 1;
  }
  return source.startsWith('.') ? 3 : 2;
}

const IMPORT_GROUPS = ['react', 'react-native', 'a package', 'a relative path'];

const importOrder = {
  meta: {
    type: 'suggestion',
    schema: [],
    messages: {
      order:
        'Import {{source}} before {{previous}}: {{group}} comes before {{after}}.',
    },
  },
  create(context) {
    return {
      Program(program) {
        let previous = null;
        for (const node of program.body) {
          if (
            node.type !== 'ImportDeclaration' ||
            node.specifiers.length === 0
          ) {
            continue;
          }
          const rank = importRank(node.source.value);
          if (previous !== null && rank < previous.rank) {
            context.report({
              node,
              messageId: 'order',
              data: {
                source: node.source.value,
                previous: previous.source,
                group: IMPORT_GROUPS[rank],
                after: IMPORT_GROUPS[previous.rank],
              },
            });
          } else {
            previous = { rank, source: node.source.value };
          }
        }
      },
    };
  },
};

const local = { rules: { 'import-order': importOrder } };

export default defineConfig([
  {
    extends: fixupConfigRules(compat.extends('@react-native', 'prettier')),
    plugins: { prettier },
    rules: {
      'react/react-in-jsx-scope': 'off',
      'prettier/prettier': 'error',
    },
  },
  {
    files: ['**/*.ts', '**/*.tsx'],
    plugins: { local },
    rules: {
      'no-duplicate-imports': 'error',
      'local/import-order': 'error',
      '@typescript-eslint/no-unused-vars': [
        'error',
        {
          varsIgnorePattern: '^_',
          argsIgnorePattern: '^_',
          ignoreRestSiblings: true,
        },
      ],
    },
  },
  {
    ignores: [
      '**/node_modules/',
      '**/lib/',
      '**/dist/',
      '**/build/',
      '**/.yarn/',
      '**/coverage/',
      '.claude/',
      'templates/shadowlist-fabric-example/android/',
      'templates/shadowlist-fabric-example/ios/',
      'templates/shadowlist-fabric-example/vendor/',
      'templates/shadowlist-macos-example/macos/',
      '**/*.config.{js,cjs,mjs}',
      '**/babel.config.js',
      '**/metro.config.js',
      '**/react-native.config.js',
      'scripts/**/*.mjs',
      'packages/shadowhash/bench/',
      'eslint.config.mjs',
      'templates/shadowlist-fabric-example/index.js',
      'templates/shadowlist-macos-example/index.js',
      'packages/shadowlist-fabric/scripts/',
    ],
  },
]);
