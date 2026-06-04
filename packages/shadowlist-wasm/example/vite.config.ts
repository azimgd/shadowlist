import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath, URL } from 'node:url';

// Resolve the library straight from source so the example always tracks the
// in-repo build of shadowlist-wasm (and its compiled .wasm) without a publish.
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      'shadowlist-wasm': fileURLToPath(new URL('../src/index.ts', import.meta.url)),
    },
  },
  server: {
    port: 5173,
    fs: {
      // Allow importing from the parent package (src + compiled wasm).
      allow: [fileURLToPath(new URL('..', import.meta.url))],
    },
  },
});
