import assert from 'node:assert/strict';

import {
  createHashStore,
  decodePost,
  decodeText,
  encodePost,
  encodeText,
  measureText,
  memoryStorage,
  placeholderText,
} from '../src';

const LONG =
  'Boarding for the 6:40 to Reykjavik started twenty minutes late, and nobody at gate B14 seemed to mind.\n\n' +
  'Low cloud over Keflavik, light rain, wind gusting from the southwest. The pilot came on before pushback ' +
  'and promised a bumpy final approach but a smooth cruise, which is exactly the kind of honesty I want at ' +
  'thirty-seven thousand feet. The cabin went quiet as we taxied.';

test('a short text keeps every word length and break', () => {
  const text = "Just shipped the new grid, it's fast\nmore soon";
  const hash = encodeText(text);
  assert.deepEqual(decodeText(hash), measureText(text));
  assert.ok(hash.length <= 10, `${hash} is ${hash.length} characters`);
});

test('the placeholder has the same word lengths as the text', () => {
  const text = 'Stuck on the tarmac in Denver—third de-icing pass.';
  const placeholder = placeholderText(decodeText(encodeText(text))!);
  assert.deepEqual(
    placeholder.split(' ').map((word) => word.length),
    text.split(' ').map((word) => [...word].length)
  );
});

test('words of 14, 15 and 40 characters round trip', () => {
  const text = `${'a'.repeat(14)} ${'b'.repeat(15)} ${'c'.repeat(40)}`;
  assert.deepEqual(decodeText(encodeText(text)), {
    kind: 'words',
    tokens: [14, 15, 40],
  });
});

test('a long text past the cap keeps its word count, breaks and about its letters', () => {
  const real = measureText(LONG);
  const shape = decodeText(encodeText(LONG, { maxNibbles: 24 }))!;
  assert.equal(shape.kind, 'words');
  if (shape.kind !== 'words' || real.kind !== 'words') return;
  const count = (tokens: number[]) => tokens.filter((t) => t > 0).length;
  const letters = (tokens: number[]) => tokens.reduce((sum, t) => sum + t, 0);
  assert.equal(count(shape.tokens), count(real.tokens));
  assert.equal(shape.tokens.length, real.tokens.length);
  // The blank line between paragraphs stays a blank line, not two single breaks apart.
  const blankLines = (tokens: number[]) =>
    tokens.filter((t, i) => t === 0 && tokens[i + 1] === 0).length;
  assert.equal(blankLines(shape.tokens), blankLines(real.tokens));
  assert.ok(
    Math.abs(letters(shape.tokens) - letters(real.tokens)) /
      letters(real.tokens) <
      0.1
  );
  assert.ok(encodeText(LONG, { maxNibbles: 24 }).length <= 24);
});

test('text without spaces keeps only its length', () => {
  assert.deepEqual(decodeText(encodeText('今日は良い天気ですね')), {
    kind: 'chars',
    length: 10,
  });
});

test('a post round trips author lengths, image shapes and text', () => {
  const hash = encodePost({
    author: { name: 'Ada Lovelace', handle: '@ada' },
    text: 'Day two in Lisbon.',
    images: [{ width: 1600, height: 900, color: '#3366cc' }, {}],
  });
  const shape = decodePost(hash);
  assert.deepEqual(shape.author, { name: 12, handle: 4 });
  assert.equal(shape.images.length, 2);
  assert.ok(Math.abs(shape.images[0]!.aspectRatio! - 16 / 9) < 0.03);
  assert.equal(shape.images[0]!.color, '#3165ce');
  assert.equal(shape.images[1]!.aspectRatio, undefined);
  assert.deepEqual(shape.text, measureText('Day two in Lisbon.'));
  assert.ok(hash.length <= 20, `${hash} is ${hash.length} characters`);
});

test('a post without text or images is four bytes', () => {
  const hash = encodePost({ author: { name: 'Bo' } });
  assert.equal(hash.length, 6);
  assert.deepEqual(decodePost(hash), {
    author: { name: 2, handle: 0 },
    images: [],
  });
});

test('a hash from a newer version or with a bad character is refused', () => {
  assert.throws(() => decodePost('8AAA'), /version/);
  assert.throws(() => decodePost('a+b'), /bad character/);
  assert.throws(() => decodePost(''), /too short/);
});

test('the store keeps the first rows of a list and ignores what it cannot read', () => {
  const storage = memoryStorage();
  const store = createHashStore(storage, { limit: 2 });
  store.remember('feed', [
    { id: 'a', hash: 'x' },
    { id: 'b', hash: 'y' },
    { id: 'c', hash: 'z' },
  ]);
  assert.deepEqual(store.recall('feed'), [
    { id: 'a', hash: 'x' },
    { id: 'b', hash: 'y' },
  ]);
  storage.set('shadowhash:broken', '{');
  assert.deepEqual(store.recall('broken'), []);
  store.forget('feed');
  assert.deepEqual(store.recall('feed'), []);
});
