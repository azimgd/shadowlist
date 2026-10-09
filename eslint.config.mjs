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
 * Single repo-wide flat config: React Native preset + Prettier, applied across
 * every workspace.
 */
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
    ignores: [
      '**/node_modules/',
      '**/lib/',
      '**/dist/',
      '**/build/',
      '**/.yarn/',
      '**/coverage/',
      '.claude/',
      // Native example build artifacts.
      'templates/shadowlist-fabric-example/android/',
      'templates/shadowlist-fabric-example/ios/',
      'templates/shadowlist-fabric-example/vendor/',
      'templates/shadowlist-macos-example/macos/',
      // Tooling / entry / build JS (not application source).
      '**/*.config.{js,cjs,mjs}',
      '**/babel.config.js',
      '**/metro.config.js',
      '**/react-native.config.js',
      'scripts/**/*.mjs',
      'eslint.config.mjs',
      'templates/shadowlist-fabric-example/index.js',
      'templates/shadowlist-macos-example/index.js',
      'packages/shadowlist-fabric/scripts/',
    ],
  },
]);
