/* eslint-disable no-bitwise */
import { ByteReader, ByteWriter, InvalidHashError } from './bytes';

/*
 * The shape of a text without its words: the length of each word and where the paragraphs
 * break. No line breaks are stored, so each device wraps the placeholder at its own width with
 * its own font, and it wraps where the real text will.
 */
export type TextShape =
  // Word lengths in characters, 0 for a line break.
  | { kind: 'words'; tokens: number[] }
  // Scripts without spaces between words, such as Chinese or Japanese, keep only a length.
  | { kind: 'chars'; length: number };

export interface TextOptions {
  // Nibbles kept word by word. Past them only the word count, break count and mean length are.
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
 * A word of length n as nibbles. 15 stands for 14 characters with more to follow, so a nibble
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
  writer.varint(words.length);
  writer.varint(rest.length - words.length);
  // Mean word length in sixteenths, up to 15.9 characters.
  writer.byte(
    Math.min(255, Math.round((letters / Math.max(1, words.length)) * 16))
  );
}

// Spread around the mean so the filled-in words do not read as a grid.
const SPREAD = [0, 2, -1, 1, -2, 0, 3, -1, -2, 1];

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
    const breaks = reader.varint();
    const mean = reader.byte() / 16;
    // One break after every this many words, so the paragraphs come out evenly sized.
    const every =
      breaks > 0 ? Math.max(1, Math.floor(words / (breaks + 1))) : Infinity;
    let placedBreaks = 0;
    for (let index = 0; index < words; index++) {
      if (index > 0 && index % every === 0 && placedBreaks < breaks) {
        tokens.push(0);
        placedBreaks++;
      }
      tokens.push(
        Math.max(1, Math.round(mean + SPREAD[index % SPREAD.length]!))
      );
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

// Letters of about the mean width in most proportional fonts, so lines fill as real words do.
const LETTERS = 'etaoinshrdlucemo';
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
