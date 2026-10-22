/* eslint-disable no-bitwise */
import { ByteReader, ByteWriter, InvalidHashError } from './bytes';
import { readText, writeText, type TextOptions, type TextShape } from './text';

/*
 * A feed post's hash: the author's name and handle lengths, the text's shape and each image's
 * aspect ratio and mean colour. A post takes 12 to 45 bytes, and a feed page can carry the
 * hashes of the page after it.
 */
export interface PostInput {
  author?: { name: string; handle?: string };
  text?: string;
  images?: ReadonlyArray<{ width?: number; height?: number; color?: string }>;
}

/*
 * An image's aspectRatio is its width over height, undefined when the post did not know it. Its
 * color is #rrggbb, rounded to 16 bits.
 */
export interface ImageShape {
  aspectRatio?: number;
  color?: string;
}

export interface PostShape {
  author?: { name: number; handle: number };
  text?: TextShape;
  images: ImageShape[];
}

export const VERSION = 1;

const Flag = { Author: 1, Images: 2, Colors: 4 } as const;

// Aspect ratios from 1/8 to 8, log scaled into 1 to 255. 0 means unknown.
const LOG_RANGE = 3;

function writeAspect(
  writer: ByteWriter,
  width?: number,
  height?: number
): void {
  if (!width || !height) {
    writer.byte(0);
    return;
  }
  const log = Math.max(
    -LOG_RANGE,
    Math.min(LOG_RANGE, Math.log2(width / height))
  );
  writer.byte(1 + Math.round(((log + LOG_RANGE) / (2 * LOG_RANGE)) * 254));
}

function readAspect(reader: ByteReader): number | undefined {
  const value = reader.byte();
  if (value === 0) return undefined;
  return 2 ** (((value - 1) / 254) * 2 * LOG_RANGE - LOG_RANGE);
}

function writeColor(writer: ByteWriter, color: string | undefined): void {
  const match = color && /^#?([0-9a-f]{6})$/i.exec(color);
  const rgb = match ? parseInt(match[1]!, 16) : 0;
  const r = (rgb >> 19) & 0x1f;
  const g = (rgb >> 10) & 0x3f;
  const b = (rgb >> 3) & 0x1f;
  const packed = (r << 11) | (g << 5) | b;
  writer.byte(packed >> 8);
  writer.byte(packed);
}

function readColor(reader: ByteReader): string {
  const packed = (reader.byte() << 8) | reader.byte();
  const r = Math.round((((packed >> 11) & 0x1f) * 255) / 31);
  const g = Math.round((((packed >> 5) & 0x3f) * 255) / 63);
  const b = Math.round(((packed & 0x1f) * 255) / 31);
  return `#${((r << 16) | (g << 8) | b).toString(16).padStart(6, '0')}`;
}

export function encodePost(post: PostInput, options?: TextOptions): string {
  const images = post.images ?? [];
  const colors = images.some((image) => image.color !== undefined);
  const writer = new ByteWriter();
  writer.byte(
    (VERSION << 4) |
      (post.author ? Flag.Author : 0) |
      (images.length ? Flag.Images : 0) |
      (colors ? Flag.Colors : 0)
  );
  if (post.author) {
    writer.varint([...post.author.name].length);
    writer.varint(post.author.handle ? [...post.author.handle].length : 0);
  }
  if (images.length) {
    writer.varint(images.length);
    for (const image of images) {
      writeAspect(writer, image.width, image.height);
      if (colors) writeColor(writer, image.color);
    }
  }
  writeText(writer, post.text, options);
  return writer.toBase64Url();
}

export function decodePost(hash: string): PostShape {
  const reader = new ByteReader(hash);
  const header = reader.byte();
  if (header >> 4 !== VERSION) {
    throw new InvalidHashError(`version ${header >> 4}, expected ${VERSION}`);
  }
  const shape: PostShape = { images: [] };
  if (header & Flag.Author) {
    shape.author = { name: reader.varint(), handle: reader.varint() };
  }
  if (header & Flag.Images) {
    const count = reader.varint();
    for (let index = 0; index < count; index++) {
      const aspectRatio = readAspect(reader);
      const color = header & Flag.Colors ? readColor(reader) : undefined;
      shape.images.push({
        ...(aspectRatio === undefined ? {} : { aspectRatio }),
        ...(color === undefined ? {} : { color }),
      });
    }
  }
  const text = readText(reader);
  if (text) shape.text = text;
  return shape;
}
