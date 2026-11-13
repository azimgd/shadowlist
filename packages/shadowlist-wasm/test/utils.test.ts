/*
 * The optional utility binary through its published subpath, replaying the native vectors.
 */
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { test } from 'node:test';

import {
  avatarColorIndex,
  avatarInitials,
  isShadowListUtilsLoaded,
  loadShadowListUtils,
  pollPercent,
  pollTally,
  relativeTime,
  relativeTimeHasDate,
} from 'shadowlist-wasm/utils';

import { isShadowListKitLoaded } from '../dist/ShadowListKitCore.js';

const require = createRequire(import.meta.url);
const UNLINKABLE = new Uint8Array([
  0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 2, 7, 1, 1, 120, 1, 121, 0, 0,
]);

async function vectors<T>(name: string): Promise<T[]> {
  return JSON.parse(
    await readFile(
      new URL(
        `../../shadowlist-core-tests/vectors/${name}.json`,
        import.meta.url
      ),
      'utf8'
    )
  ) as T[];
}

test('optional utilities reject failed loads, retry and share one attempt without loading the list', async () => {
  assert.throws(() => avatarInitials('Ada'), /loadShadowListUtils/);
  await assert.rejects(
    loadShadowListUtils({ source: new Uint8Array([0, 1, 2, 3]) })
  );
  assert.equal(isShadowListUtilsLoaded(), false);
  await assert.rejects(
    loadShadowListUtils({ source: await WebAssembly.compile(UNLINKABLE) })
  );
  assert.equal(isShadowListUtilsLoaded(), false);
  const source = await WebAssembly.compile(
    await readFile(require.resolve('shadowlist-wasm/shadowlistutils.wasm'))
  );
  const attempt = loadShadowListUtils({ source });
  assert.equal(loadShadowListUtils(), attempt);
  await attempt;
  assert.equal(isShadowListUtilsLoaded(), true);
  assert.equal(isShadowListKitLoaded(), false);
});

test('avatar names preserve native UTF-16 colors and UTF-8 initials', async () => {
  for (const row of await vectors<{
    name: string;
    input: { name?: string; units?: number[]; paletteSize: number };
    output: { colorIndex: number; initials: string };
  }>('avatar')) {
    const name = row.input.name ?? String.fromCharCode(...row.input.units!);
    assert.equal(
      avatarColorIndex(name, row.input.paletteSize),
      row.output.colorIndex,
      row.name
    );
    assert.equal(avatarInitials(name), row.output.initials, row.name);
  }
  const longName = '🦊' + 'a'.repeat(100_000) + ' 李';
  assert.equal(avatarInitials(longName), '🦊李');
  assert.equal(avatarInitials('Ada Lovelace'), 'AL');
});

test('relative-time buckets replay shared boundaries and leave date formatting to the host', async () => {
  for (const row of await vectors<{
    name: string;
    input: { dateMs: number; nowMs: number };
    output: { unit: string; count: number };
  }>('relative-time')) {
    assert.deepEqual(
      relativeTime(row.input.dateMs, row.input.nowMs),
      row.output,
      row.name
    );
  }
  assert.deepEqual(relativeTime(NaN, 0), { unit: 'date', count: 0 });
  assert.equal(relativeTimeHasDate(NaN), false);
  assert.equal(relativeTimeHasDate(Infinity), false);
  assert.equal(relativeTimeHasDate(0), true);
});

test('poll totals, first leaders and whole percentages replay shared vectors', async () => {
  for (const row of await vectors<{
    name: string;
    input: { votes: number[] };
    output: { totalVotes: number; leadingIndex: number; percents: number[] };
  }>('poll')) {
    assert.deepEqual(
      pollTally(row.input.votes),
      {
        totalVotes: row.output.totalVotes,
        leadingIndex: row.output.leadingIndex,
      },
      row.name
    );
    assert.deepEqual(
      row.input.votes.map((votes) => pollPercent(votes, row.output.totalVotes)),
      row.output.percents,
      row.name
    );
  }
  assert.deepEqual(pollTally(Array.from({ length: 2_100_000 }, () => 1)), {
    totalVotes: 2_100_000,
    leadingIndex: 0,
  });
  assert.deepEqual(pollTally([]), { totalVotes: 0, leadingIndex: -1 });
});

test('the base binary exports no optional utility entry points', async () => {
  const source = await WebAssembly.compile(
    await readFile(require.resolve('shadowlist-wasm/shadowlistkit.wasm'))
  );
  assert.equal(
    WebAssembly.Module.exports(source).some((entry) =>
      entry.name.includes('ShadowListUtils')
    ),
    false
  );
});
