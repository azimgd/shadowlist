# shadowhash

Placeholders for text and feed posts, in the spirit of BlurHash. A hash keeps the length of each word, where
paragraphs break, the author's name length and each image's aspect ratio and mean colour. It keeps no line breaks,
so each device wraps the stand-in words at its own width and font, and a placeholder row is the height the real row
will be on a phone, a tablet or the web.

| Post | Hash size |
| --- | --- |
| Author, handle and one line of text | 16–20 characters |
| Author, a paragraph and one image | 30–40 characters |
| A long post, past 64 word nibbles | ≤ 60 characters |

Plain TypeScript with no dependencies. It runs in Hermes, browsers and Node.

## Usage

```ts
import { decodePost, encodePost, placeholderText } from 'shadowhash';

// Where the post is written, on a server or the device.
const hash = encodePost({
  author: { name: 'Ada Lovelace', handle: '@ada' },
  text: 'Day two in Lisbon and I have stopped using the map.',
  images: [{ width: 1600, height: 900, color: '#8a9bb0' }],
});

// Where the list draws it before the post arrives.
const shape = decodePost(hash);
const words = shape.text ? placeholderText(shape.text) : '';
```

Draw the stand-in words as bars (`RedactedText` in shadowlist-utils) or blurred, never as legible text.

## Format

```
[header] [name length, handle length] [image count, per image: aspect byte, RGB565] [text]

header  version (4 bits) | author, images, colours flags
text    varint (nibble count << 2 | mode), then nibbles
        mode 0 none, 1 words, 2 words cut short, 3 characters only (CJK)
nibble  0 line break, 1-14 word length, 15 = 14 more characters follow
cut     remaining words, remaining breaks, mean word length in sixteenths
```

Bytes are base64url, so a hash fits in JSON, a URL or a database column unescaped.

## Caching on the device

`createHashStore` keeps the first rows of a list between launches, through MMKV or any `getString`, `set` and
`remove` storage. On a cold start the list draws those rows before its first request returns.

```ts
import { createMMKV } from 'react-native-mmkv';
import { createHashStore } from 'shadowhash';

const mmkv = createMMKV({ id: 'shadowhash' });
const store = createHashStore({
  getString: (key) => mmkv.getString(key),
  set: (key, value) => mmkv.set(key, value),
  remove: (key) => {
    mmkv.remove(key);
  },
});
```

The Feed screen in `templates/shadowlist-fabric-example` uses both: cached hashes on a cold start, and each page
carries the hashes of the page after it so scrolling past the end shows rows of their real size.
