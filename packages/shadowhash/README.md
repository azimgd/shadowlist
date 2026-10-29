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

Bytes are base64url. A hash fits in JSON, a URL or a database column unescaped.

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

## Benchmarks

`bench/run.sh` measures hash sizes and throughput over a 500-post corpus, and on macOS lays each post and its
placeholder out with CoreText and counts the lines.

The stand-in letters were tuned on that 500-post corpus: the example app's 21 sample posts and 479 posts recombined
from their sentences. Fidelity is scored on a separate held-out corpus (`bench/heldout.cjs`) of 233 posts written
apart from it and never used for tuning: plain English, long multi-paragraph posts, links, emoji, numbers and
punctuation, French, Spanish, German, Polish, Vietnamese, Japanese, Chinese and Korean.

Held-out, at the feed's 15pt style and a 291pt column (iPhone SE):

| Placeholder | Real line count | Within one line |
| --- | --- | --- |
| shadowhash | 83.7% | 97.0% |
| Character count only, as five-letter words | 70.8% | 88.4% |
| Fixed three-line skeleton | 27.9% | 77.3% |

Across the 309, 346, 600 and 700pt columns shadowhash hits the real line count on 86–93% of posts and lands within
one line on 97–98%. The 24 posts past 64 word nibbles, where the hash keeps only counts and a mean word length for
the rest, hit 54–71% and land within one line on 92–100%. Links and long posts score lowest: links hit 68% at
291pt, and long posts 50%, with 80% within one line. Chinese, Japanese and Korean, kept as a character count, land
within one line on 88–90%.

The calibration corpus scores higher: 86.0% and 98.8% at 291pt, against 65.8% and 79.2% for the character count and
19.6% and 48.0% for the skeleton. It carries more long posts (93 of 500 past 64 nibbles), which is why the skeleton
does worse there. Laid out unwrapped, the stand-in letters are 0.9969 of the real text's width on the calibration
corpus and 0.9855 on the held-out one. Emoji posts (0.928) and Vietnamese (0.921) are the furthest off. After
`bench/run.sh`, `node bench/fidelity.cjs --calibration` scores the calibration corpus.

Scope: every number comes from CoreText's CTFramesetter with the macOS system font. React Native's `Text` on iOS
and Android with Roboto are not measured.
