/* eslint-disable no-bitwise */
import assert from 'node:assert/strict';

import {
  ByteWriter,
  InvalidHashError,
  LIMITS,
  decodePost,
  decodeText,
  encodePost,
  encodeText,
  placeholderText,
  type TextShape,
} from '../src';

const AUTHOR = 0x11;
const IMAGES = 0x12;

/*
 * A hash of these bytes. A number entry is one byte, and { varint } is written as the encoder
 * writes it.
 */
function hash(...parts: Array<number | { varint: number }>): string {
  const writer = new ByteWriter();
  for (const part of parts) {
    if (typeof part === 'number') writer.byte(part);
    else writer.varint(part.varint);
  }
  return writer.toBase64Url();
}

function refuses(decode: () => unknown, reason: RegExp): void {
  assert.throws(decode, (error: unknown) => {
    assert.ok(error instanceof InvalidHashError, String(error));
    assert.match(error.message, reason);
    return true;
  });
}

/*
 * Mulberry32. The fuzz runs the same inputs every time.
 */
function random(seed: number): () => number {
  let state = seed;
  return () => {
    state = (state + 0x6d2b79f5) | 0;
    let value = Math.imul(state ^ (state >>> 15), 1 | state);
    value ^= value + Math.imul(value ^ (value >>> 7), 61 | value);
    return ((value ^ (value >>> 14)) >>> 0) / 4294967296;
  };
}

function tokenCount(shape: TextShape | undefined): number {
  if (!shape) return 0;
  return shape.kind === 'words' ? shape.tokens.length : shape.length;
}

test('a number past three bytes is refused', () => {
  refuses(() => decodePost(hash(AUTHOR, 0x80, 0x80, 0x80, 0x01)), /3 bytes/);
  refuses(() => decodeText(hash(...Array(200).fill(0xff))), /3 bytes/);
});

test('a number not written the shortest way is refused', () => {
  refuses(() => decodePost(hash(AUTHOR, 0x85, 0x00, 0x00)), /shortest/);
  refuses(() => decodeText(hash(0x81, 0x00)), /shortest/);
});

test('a hash cut short is refused at every length', () => {
  const full = encodePost({
    author: { name: 'Ada Lovelace', handle: '@ada' },
    text: 'Day two in Lisbon and I have stopped using the map.',
    images: [{ width: 1600, height: 900, color: '#8a9bb0' }],
  });
  for (let length = 0; length < full.length - 1; length++) {
    refuses(() => decodePost(full.slice(0, length)), /too short/);
  }
  refuses(() => decodeText(hash({ varint: (40 << 2) | 1 }, 0x11)), /short/);
});

test('a hash past the length limit is refused before it is read', () => {
  refuses(
    () => decodePost('A'.repeat(LIMITS.hashLength + 1)),
    /1025 characters/
  );
  refuses(() => decodeText('A'.repeat(10_000_000)), /at most 1024/);
});

test('counts past their limits are refused', () => {
  const over = LIMITS.length + 1;
  refuses(() => decodePost(hash(AUTHOR, { varint: over }, 0, 0)), /name/);
  refuses(() => decodePost(hash(AUTHOR, 4, { varint: over }, 0)), /handle/);
  refuses(
    () => decodePost(hash(IMAGES, { varint: LIMITS.images + 1 })),
    /65 images/
  );
  refuses(() => decodeText(hash({ varint: (over << 2) | 3 })), /characters/);
  refuses(
    () => decodeText(hash({ varint: ((LIMITS.nibbles + 1) << 2) | 1 })),
    /nibbles/
  );
});

test('a cut text with more words or breaks than the limit is refused', () => {
  const cut = (words: number, runs: number, runLength: number) =>
    hash({ varint: 2 }, { varint: words }, { varint: runs }, runLength, 80);
  refuses(() => decodeText(cut(LIMITS.length + 1, 0, 0)), /words/);
  refuses(() => decodeText(cut(10, 12, 1)), /runs of breaks/);
  refuses(() => decodeText(cut(LIMITS.length, 1000, 255)), /breaks/);
  // Before the limits the first ran a trillion words and the second broke the placeholder.
  refuses(() => decodeText(cut(2 ** 40, 0, 0)), /3 bytes/);
  refuses(() => decodeText(hash({ varint: 2 ** 42 + 3 })), /3 bytes/);
});

test('a cut text at the limits decodes', () => {
  const shape = decodeText(
    hash({ varint: 2 }, { varint: LIMITS.length }, { varint: 256 }, 255, 80)
  );
  assert.equal(shape?.kind, 'words');
  assert.ok(tokenCount(shape) <= LIMITS.length * 2);
});

test('the encoder keeps a post inside the limits', () => {
  const longWord = 'x'.repeat(LIMITS.length + 10);
  const tail = 'ab '.repeat(LIMITS.length + 10) + '\n\n\n'.repeat(400);
  const words = Array.from({ length: 1000 }, () => 'abc').join(' ');
  const post = {
    author: { name: longWord, handle: longWord },
    text: `${words} ${tail} ${words}`,
    images: Array.from({ length: 100 }, () => ({
      width: 4,
      height: 3,
      color: '#123456',
    })),
  };
  const full = encodePost(post, { maxNibbles: 5000 });
  assert.ok(full.length <= LIMITS.hashLength, `${full.length} characters`);
  const shape = decodePost(full);
  assert.deepEqual(shape.author, {
    name: LIMITS.length,
    handle: LIMITS.length,
  });
  assert.equal(shape.images.length, LIMITS.images);
  assert.equal(shape.text?.kind, 'words');
  assert.deepEqual(decodeText(encodeText('今'.repeat(LIMITS.length * 2))), {
    kind: 'chars',
    length: LIMITS.length,
  });
});

test('random bytes decode or throw InvalidHashError, within time and size', () => {
  const next = random(7);
  const alphabet =
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  const seeds = [
    encodePost({ author: { name: 'Bo' }, text: 'one two\n\nthree' }),
    encodePost({ text: 'a '.repeat(200) }, { maxNibbles: 8 }),
    encodePost({ images: [{ width: 2, height: 1, color: '#ffffff' }] }),
    encodeText('今日は良い天気ですね'),
  ];
  const started = Date.now();
  let decoded = 0;
  let largest = 0;
  for (let round = 0; round < 20000; round++) {
    let input: string;
    if (round % 2 === 0) {
      const chars = [...seeds[round % seeds.length]!];
      const edits = 1 + Math.floor(next() * 4);
      for (let edit = 0; edit < edits; edit++) {
        chars[Math.floor(next() * chars.length)] =
          alphabet[Math.floor(next() * 64)]!;
      }
      input = chars.join('');
    } else {
      const length = Math.floor(next() * 48);
      // E starts the first byte with 0001, the post version.
      input = round % 4 === 1 ? 'E' : '';
      for (let index = 0; index < length; index++) {
        input += alphabet[Math.floor(next() * 64)];
      }
    }
    for (const decode of [
      () => decodePost(input).text,
      () => decodeText(input),
    ]) {
      try {
        const text = decode();
        decoded++;
        largest = Math.max(largest, tokenCount(text));
        if (text) placeholderText(text);
      } catch (error) {
        assert.ok(error instanceof InvalidHashError, `${input}: ${error}`);
      }
    }
  }
  assert.ok(decoded > 1000, `${decoded} decoded`);
  assert.ok(largest <= LIMITS.nibbles + LIMITS.length * 2);
  assert.ok(Date.now() - started < 10000);
});
