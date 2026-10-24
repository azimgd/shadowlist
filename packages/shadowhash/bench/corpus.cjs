/*
 * Corpus for the shadowhash bench: the example's 21 real sample posts, plus a synthetic set
 * built from their sentences with a seeded generator. The mix models a feed: many short posts,
 * some medium, a tail of long multi-paragraph ones.
 */
const fs = require('fs');
const path = require('path');

const common = path.join(
  __dirname,
  '../../../templates/shadowlist-fabric-example/src/fixtures/common.ts'
);
const source = fs.readFileSync(common, 'utf8');
// Pull SAMPLE_TEXTS out of the TS file by evaluating just the array literal.
const match = source.match(/export const SAMPLE_TEXTS = (\[[\s\S]*?\n\]);/);
const SAMPLE_TEXTS = eval(match[1]);

function mulberry32(seed) {
  let a = seed;
  return function () {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

const sentences = SAMPLE_TEXTS.flatMap((text) =>
  text
    .replace(/\n+/g, ' ')
    .split(/(?<=[.!?])\s+/)
    .filter((sentence) => sentence.length > 0)
);

function syntheticPost(rand) {
  const roll = rand();
  const sentenceCount =
    roll < 0.35
      ? 1
      : roll < 0.7
        ? 2
        : roll < 0.9
          ? 4
          : 8 + Math.floor(rand() * 6);
  const parts = [];
  for (let index = 0; index < sentenceCount; index++) {
    parts.push(sentences[Math.floor(rand() * sentences.length)]);
  }
  // Long posts get a paragraph break every three sentences.
  if (sentenceCount >= 4) {
    const out = [];
    for (let index = 0; index < parts.length; index++) {
      out.push(parts[index]);
      if (index === parts.length - 1) continue;
      out.push((index + 1) % 3 === 0 ? '\n\n' : ' ');
    }
    return out.join('');
  }
  return parts.join(' ');
}

function buildCorpus(count = 500, seed = 42) {
  const rand = mulberry32(seed);
  const posts = SAMPLE_TEXTS.map((text, index) => ({
    id: `real-${index}`,
    text,
  }));
  for (let index = 0; index < count - SAMPLE_TEXTS.length; index++) {
    posts.push({ id: `syn-${index}`, text: syntheticPost(rand) });
  }
  return posts;
}

module.exports = { SAMPLE_TEXTS, buildCorpus };
