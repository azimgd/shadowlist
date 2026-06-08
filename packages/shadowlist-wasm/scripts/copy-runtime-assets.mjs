import { cpSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const packageDir = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const sourceDir = resolve(packageDir, 'src', 'wasm');
const outputDir = resolve(packageDir, 'dist', 'wasm');

mkdirSync(outputDir, { recursive: true });
cpSync(resolve(sourceDir, 'shadowlistCore.wasm'), outputDir + '/shadowlistCore.wasm');
cpSync(resolve(sourceDir, 'shadowlistCore.d.ts'), outputDir + '/shadowlistCore.d.ts');
