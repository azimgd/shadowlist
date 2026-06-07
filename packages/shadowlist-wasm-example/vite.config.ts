import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath, URL } from 'node:url';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: [
      {
        find: 'shadowlist-utils/web',
        replacement: fileURLToPath(
          new URL('../shadowlist-utils/src/web/index.ts', import.meta.url)
        ),
      },
      {
        find: 'shadowlist-utils',
        replacement: fileURLToPath(
          new URL('../shadowlist-utils/src/index.ts', import.meta.url)
        ),
      },
      {
        find: 'shadowlist-wasm',
        replacement: fileURLToPath(
          new URL('../shadowlist-wasm/src/index.ts', import.meta.url)
        ),
      },
    ],
  },
  server: {
    port: 5173,
    fs: {
      allow: [
        fileURLToPath(new URL('../shadowlist-wasm', import.meta.url)),
        fileURLToPath(new URL('../shadowlist-utils', import.meta.url)),
      ],
    },
  },
});
