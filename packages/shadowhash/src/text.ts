/* eslint-disable no-bitwise */
import { ByteReader, ByteWriter, InvalidHashError } from './bytes';

/*
 * The shape of a text without its words: the length of each word and where the paragraphs
 * break. No line breaks are stored. Each device wraps the placeholder at its own width with its
 * own font, and it wraps where the real text will. A words shape keeps word lengths in
 * characters, 0 for a line break. Scripts without spaces between words, such as Chinese or
 * Japanese, keep only a length.
 */
export type TextShape =
  { kind: 'words'; tokens: number[] } | { kind: 'chars'; length: number };

/*
 * maxNibbles is the number of nibbles kept word by word. Past them only the word count, break
 * count and mean length are.
 */
export interface TextOptions {
  maxNibbles?: number;
}

const DEFAULT_MAX_NIBBLES = 64;
const NO_SPACES = /[぀-ヿ㐀-鿿豈-﫿가-힯]/;

// The low two bits of a text's first varint.
const Mode = { None: 0, Words: 1, WordsTruncated: 2, Chars: 3 } as const;

export function measureText(text: string): TextShape {
  if (NO_SPACES.test(text)) {
    return { kind: 'chars', length: [...text.replace(/\s+/g, '')].length };
  }
  const tokens: number[] = [];
  text.split('\n').forEach((line, index) => {
    if (index > 0) tokens.push(0);
    for (const word of line.match(/\S+/g) ?? []) tokens.push([...word].length);
  });
  return { kind: 'words', tokens };
}

/*
 * A word of length n as nibbles. 15 stands for 14 characters with more to follow. A nibble
 * after a 15 is never 0 and never reads as a break.
 */
function wordNibbles(length: number): number[] {
  const out: number[] = [];
  let rest = length;
  while (rest > 14) {
    out.push(15);
    rest -= 14;
  }
  out.push(rest);
  return out;
}

export function writeText(
  writer: ByteWriter,
  text: string | undefined,
  { maxNibbles = DEFAULT_MAX_NIBBLES }: TextOptions = {}
): void {
  if (text === undefined || text.trim().length === 0) {
    writer.varint(Mode.None);
    return;
  }
  const shape = measureText(text);
  if (shape.kind === 'chars') {
    writer.varint((shape.length << 2) | Mode.Chars);
    return;
  }
  const nibbles: number[] = [];
  let kept = 0;
  for (const token of shape.tokens) {
    const next = token === 0 ? [0] : wordNibbles(token);
    if (nibbles.length + next.length > maxNibbles) break;
    nibbles.push(...next);
    kept++;
  }
  const rest = shape.tokens.slice(kept);
  writer.varint(
    (nibbles.length << 2) | (rest.length ? Mode.WordsTruncated : Mode.Words)
  );
  writer.nibbles(nibbles);
  if (rest.length === 0) return;
  const words = rest.filter((token) => token > 0);
  const letters = words.reduce((sum, token) => sum + token, 0);
  // Breaks come in runs, one for a new line and two for a blank line between paragraphs.
  const runs = rest.filter(
    (token, index) => token === 0 && rest[index - 1] !== 0
  ).length;
  writer.varint(words.length);
  writer.varint(runs);
  if (runs > 0) {
    writer.byte(Math.round((rest.length - words.length) / runs));
  }
  // Mean word length in sixteenths, up to 15.9 characters.
  writer.byte(
    Math.min(255, Math.round((letters / Math.max(1, words.length)) * 16))
  );
}

// Spread around the mean so the filled-in words do not read as a grid. It sums to zero.
const SPREAD = [0, 2, -1, 1, -2, 0, 3, -1, -2, 0];

export function readText(reader: ByteReader): TextShape | undefined {
  const head = reader.varint();
  const mode = head & 3;
  const count = Math.floor(head / 4);
  if (mode === Mode.None) return undefined;
  if (mode === Mode.Chars) return { kind: 'chars', length: count };

  const tokens: number[] = [];
  let carry = 0;
  for (const nibble of reader.nibbles(count)) {
    if (nibble === 15) {
      carry += 14;
      continue;
    }
    if (nibble === 0 && carry === 0) tokens.push(0);
    else tokens.push(carry + nibble);
    carry = 0;
  }
  if (carry > 0) throw new InvalidHashError('a long word runs past the end');
  if (mode === Mode.WordsTruncated) {
    const words = reader.varint();
    const runs = reader.varint();
    const runLength = runs > 0 ? reader.byte() : 0;
    const mean = reader.byte() / 16;
    // One run of breaks after every this many words keeps the paragraphs evenly sized.
    const every =
      runs > 0 ? Math.max(1, Math.floor(words / (runs + 1))) : Infinity;
    let placedRuns = 0;
    /*
     * The rounding error carries into the next word. The filled-in words keep the tail's total
     * letter count, and the placeholder wraps to the real text's line count more often.
     */
    let rounding = 0;
    for (let index = 0; index < words; index++) {
      if (index > 0 && index % every === 0 && placedRuns < runs) {
        for (let line = 0; line < runLength; line++) tokens.push(0);
        placedRuns++;
      }
      const ideal = mean + SPREAD[index % SPREAD.length]! + rounding;
      const token = Math.max(1, Math.round(ideal));
      rounding = ideal - token;
      tokens.push(token);
    }
  }
  return { kind: 'words', tokens };
}

export function encodeText(text: string, options?: TextOptions): string {
  const writer = new ByteWriter();
  writeText(writer, text, options);
  return writer.toBase64Url();
}

export function decodeText(hash: string): TextShape | undefined {
  return readText(new ByteReader(hash));
}

/*
 * Letters about as wide on average as running text with its capitals and punctuation. Lines fill
 * and wrap as the real words do. Measured against CoreText at 15pt, these fourteen average
 * 0.9997 of the real corpus's width per character where etaoinshrdluce sat 1.5% narrow and
 * wrapped a line short. The glyphs are never shown: only their advance widths matter.
 */
const LETTERS = 'etaoinshrdfuce';
const WIDE_GLYPH = '口';

/*
 * Stand-in words for a shape: as many letters per word as the real text and a newline where it
 * breaks. Draw them blurred or as bars, never as legible text.
 */
export function placeholderWords(shape: TextShape): string[] {
  if (shape.kind === 'chars') return [WIDE_GLYPH.repeat(shape.length)];
  let cursor = 0;
  return shape.tokens.map((length) => {
    if (length === 0) return '\n';
    let word = '';
    for (let index = 0; index < length; index++) {
      word += LETTERS[cursor++ % LETTERS.length];
    }
    return word;
  });
}

export function placeholderText(shape: TextShape): string {
  return placeholderWords(shape)
    .join(' ')
    .replace(/ ?\n ?/g, '\n');
}
