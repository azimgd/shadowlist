import { expect } from '@jest/globals';
import { readFileSync, readdirSync, writeFileSync } from 'fs';
import { join } from 'path';

/*
 * One case of a vector file in packages/shadowlist-core-tests/vectors, the list engine's
 * folder. The files are the contract between a function in TypeScript and its twin in
 * packages/shadowlist-core: jest replays them through the TypeScript and the core tests replay
 * them through the C++. function names which one a case is for, in a file that holds more than
 * one. The template libraries of the shadowlist-templates repository keep their own files.
 */
export interface VectorCase {
  name: string;
  function?: string;
  input: unknown;
  output: unknown;
}

const VECTORS_DIR = join(__dirname, '../../../shadowlist-core-tests/vectors');

/*
 * The files vectors.test replays. A file in the folder that is not here fails the test, and so
 * does a file here that the test does not replay.
 */
const VECTOR_FILES = ['mounted-range.json'] as const;

const replayedFiles = new Set<string>();

/*
 * Text outside ASCII is written as escapes. A no-break space and a line separator then show
 * in a diff.
 */
function serialize(cases: ReadonlyArray<VectorCase>): string {
  const json = JSON.stringify(cases, null, 2).replace(
    /[\u007f-￿]/g,
    (unit) => `\\u${unit.charCodeAt(0).toString(16).padStart(4, '0')}`
  );
  return `${json}\n`;
}

/*
 * The cases of a file with the outputs run gives them. What JSON keeps of a result: no
 * undefined fields, and -0 as 0.
 */
function replay(
  cases: ReadonlyArray<VectorCase>,
  run: (vector: VectorCase) => unknown
): VectorCase[] {
  return cases.map((vector) => ({
    ...vector,
    output: JSON.parse(JSON.stringify(run(vector) ?? null)) as unknown,
  }));
}

/*
 * Runs every case of a vector file through run and checks the file's output. run takes the
 * case, its input and its output in the file. With UPDATE_VECTORS=1 the outputs are written to
 * the file first, and the file is then read and checked like in any other run.
 */
export function replayVectors(
  file: string,
  run: (vector: VectorCase) => unknown
): void {
  replayedFiles.add(file);
  const path = join(VECTORS_DIR, file);
  const read = () => JSON.parse(readFileSync(path, 'utf8')) as VectorCase[];
  if (process.env.UPDATE_VECTORS === '1') {
    writeFileSync(path, serialize(replay(read(), run)));
  }
  const cases = read();
  const replayed = replay(cases, run);
  expect(cases.length).toBeGreaterThan(0);
  for (const [index, vector] of cases.entries()) {
    expect({
      name: vector.name,
      output: replayed[index]!.output,
    }).toStrictEqual({ name: vector.name, output: vector.output });
  }
}

/*
 * Checks that the folder holds the files in VECTOR_FILES and that the test file calling this
 * replayed every one of them. Call it after the replays ran.
 */
export function expectEveryVectorFileReplayed(): void {
  const known = [...VECTOR_FILES].sort();
  const inFolder = readdirSync(VECTORS_DIR)
    .filter((name) => name.endsWith('.json'))
    .sort();
  expect(inFolder).toStrictEqual(known);
  expect([...replayedFiles].sort()).toStrictEqual(known);
}
