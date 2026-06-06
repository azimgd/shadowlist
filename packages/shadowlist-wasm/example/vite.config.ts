import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath, URL } from 'node:url';

// Resolve the library from source so the example tracks the in-repo build.
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      'shadowlist-wasm': fileURLToPath(new URL('../src/index.ts', import.meta.url)),
      // Shared example helpers, resolved from source.
      'shadowlist-utils': fileURLToPath(
        new URL('../../shadowlist-utils/src/index.ts', import.meta.url)
      ),
    },
  },
  server: {
    port: 5173,
    fs: {
      // Allow importing from the parent package and shadowlist-utils.
      allow: [
        fileURLToPath(new URL('..', import.meta.url)),
        fileURLToPath(new URL('../../shadowlist-utils', import.meta.url)),
      ],
    },
  },
});
