/* eslint-disable no-bitwise */
/*
 * The byte stream every hash is written into, and base64url without btoa or Buffer. The same
 * code runs in Hermes, a browser and Node.
 */
const ALPHABET =
  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';

const LOOKUP = new Map([...ALPHABET].map((char, index) => [char, index]));

/*
 * The most a decoder reads or makes from one hash. A hash the encoder writes stays inside them,
 * and the longest it can write is 963 characters. hashLength is in base64url characters,
 * images the image count, nibbles the word nibbles of a text and length every count that turns
 * into characters or words: a name, a handle, a text without spaces and the words past the
 * nibbles. varintBytes holds the largest of those counts.
 */
export const LIMITS = {
  hashLength: 1024,
  images: 64,
  nibbles: 1024,
  length: 65536,
  varintBytes: 3,
} as const;

export class ByteWriter {
  private readonly bytes: number[] = [];

  byte(value: number): void {
    this.bytes.push(value & 0xff);
  }

  /*
   * Seven bits per byte, low first. Lengths under 128 take one byte.
   */
  varint(value: number): void {
    let rest = Math.max(0, Math.floor(value));
    while (rest >= 0x80) {
      this.bytes.push((rest & 0x7f) | 0x80);
      rest = Math.floor(rest / 0x80);
    }
    this.bytes.push(rest);
  }

  /*
   * Two nibbles per byte, high first. An odd count leaves the last low nibble 0.
   */
  nibbles(values: ReadonlyArray<number>): void {
    for (let index = 0; index < values.length; index += 2) {
      this.byte(
        ((values[index]! & 0xf) << 4) | ((values[index + 1] ?? 0) & 0xf)
      );
    }
  }

  toBase64Url(): string {
    let out = '';
    for (let index = 0; index < this.bytes.length; index += 3) {
      const a = this.bytes[index]!;
      const b = this.bytes[index + 1];
      const c = this.bytes[index + 2];
      const chunk = (a << 16) | ((b ?? 0) << 8) | (c ?? 0);
      out += ALPHABET[(chunk >> 18) & 63]! + ALPHABET[(chunk >> 12) & 63]!;
      if (b !== undefined) out += ALPHABET[(chunk >> 6) & 63]!;
      if (c !== undefined) out += ALPHABET[chunk & 63]!;
    }
    return out;
  }
}

export class InvalidHashError extends Error {
  constructor(reason: string) {
    super(`Invalid shadowhash: ${reason}.`);
    this.name = 'InvalidHashError';
  }
}

export class ByteReader {
  private readonly bytes: number[] = [];
  private offset = 0;

  constructor(hash: string) {
    if (hash.length > LIMITS.hashLength) {
      throw new InvalidHashError(
        `${hash.length} characters, at most ${LIMITS.hashLength}`
      );
    }
    let bits = 0;
    let bitCount = 0;
    for (const char of hash) {
      const value = LOOKUP.get(char);
      if (value === undefined)
        throw new InvalidHashError(`bad character "${char}"`);
      bits = ((bits << 6) | value) & 0xffffff;
      bitCount += 6;
      if (bitCount >= 8) {
        bitCount -= 8;
        this.bytes.push((bits >> bitCount) & 0xff);
      }
    }
  }

  byte(): number {
    const value = this.bytes[this.offset++];
    if (value === undefined) throw new InvalidHashError('too short');
    return value;
  }

  get remaining(): number {
    return this.bytes.length - this.offset;
  }

  /*
   * At most LIMITS.varintBytes bytes, written the shortest way: a last byte of 0 after another
   * byte is refused, as the writer never ends one with it.
   */
  varint(): number {
    let value = 0;
    for (let index = 0; index < LIMITS.varintBytes; index++) {
      const next = this.byte();
      value |= (next & 0x7f) << (7 * index);
      if ((next & 0x80) === 0) {
        if (next === 0 && index > 0)
          throw new InvalidHashError(
            'a number is not written the shortest way'
          );
        return value;
      }
    }
    throw new InvalidHashError(
      `a number runs past ${LIMITS.varintBytes} bytes`
    );
  }

  /*
   * A varint that counts something, refused past max.
   */
  count(max: number, what: string): number {
    const value = this.varint();
    if (value > max) {
      throw new InvalidHashError(`${value} ${what}, at most ${max}`);
    }
    return value;
  }

  nibbles(count: number): number[] {
    if (count > this.remaining * 2) throw new InvalidHashError('too short');
    const values: number[] = [];
    while (values.length < count) {
      const next = this.byte();
      values.push(next >> 4);
      if (values.length < count) values.push(next & 0xf);
    }
    return values;
  }
}
