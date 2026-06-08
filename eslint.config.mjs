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

// Single repo-wide flat config: React Native preset + Prettier, applied across
// every workspace. typescript-eslint disables core no-undef for TS, so the same
// preset works for the DOM (web/wasm) packages without browser-globals wiring.
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
    files: [
      'packages/shadowlist-wasm/**/*.{ts,tsx}',
      'packages/shadowlist-wasm-example/**/*.{ts,tsx}',
      'packages/shadowlist-utils/src/web/**/*.{ts,tsx}',
    ],
    rules: {
      'react-native/no-inline-styles': 'off',
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
      // Generated (emscripten) WASM glue + binaries.
      'packages/shadowlist-wasm/src/wasm/',
      '**/*.wasm',
      // Native example build artifacts.
      'packages/shadowlist-fabric-example/android/',
      'packages/shadowlist-fabric-example/ios/',
      // Tooling / entry / build JS (not application source).
      '**/*.config.{js,cjs,mjs}',
      '**/babel.config.js',
      '**/metro.config.js',
      '**/react-native.config.js',
      '**/jest.config.js',
      'eslint.config.mjs',
      'packages/shadowlist-fabric-example/index.js',
      'packages/shadowlist-fabric/scripts/',
      'packages/shadowlist-wasm/scripts/',
    ],
  },
]);
