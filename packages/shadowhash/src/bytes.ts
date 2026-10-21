/* eslint-disable no-bitwise */
/*
 * The byte stream every hash is written into, and base64url without btoa or Buffer, so the same
 * code runs in Hermes, a browser and Node.
 */
const ALPHABET =
  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';

const LOOKUP = new Map([...ALPHABET].map((char, index) => [char, index]));

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

  varint(): number {
    let value = 0;
    let scale = 1;
    for (;;) {
      const next = this.byte();
      value += (next & 0x7f) * scale;
      if ((next & 0x80) === 0) return value;
      scale *= 0x80;
    }
  }

  nibbles(count: number): number[] {
    const values: number[] = [];
    while (values.length < count) {
      const next = this.byte();
      values.push(next >> 4);
      if (values.length < count) values.push(next & 0xf);
    }
    return values;
  }
}
