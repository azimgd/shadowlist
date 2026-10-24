/*
 * Size and throughput over the corpus: hash bytes per post against the JSON post and gzip,
 * encode/decode ops per second, and the two production moments end to end. The example app
 * pays the cold-start cost once per launch and the page cost once per fetched page.
 */
const zlib = require('zlib');
const { buildCorpus } = require('./corpus.cjs');
const sh = require('./dist/index.js');

const NAMES = [
  'Mara Quinn',
  'Theo Alvarez',
  'Priya Nair',
  'Elena Sokolova',
  'Wren Takahashi',
];

const corpus = buildCorpus(500).map((post, index) => {
  const name = NAMES[index % NAMES.length];
  const images =
    index % 10 === 0
      ? [
          { width: 1600, height: 900, color: '#8a9bb0' },
          { width: 800, height: 1000, color: '#c2a27c' },
          { width: 1200, height: 800, color: '#6f8f72' },
        ]
      : index % 3 === 0
        ? [{ width: 800, height: 600, color: '#8a9bb0' }]
        : [];
  return {
    id: post.id,
    input: {
      author: { name, handle: `@${name.toLowerCase().replace(/\s+/g, '')}` },
      text: post.text,
      images,
    },
  };
});

function percentile(values, p) {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[
    Math.min(sorted.length - 1, Math.floor((p / 100) * sorted.length))
  ];
}
const mean = (values) => values.reduce((a, b) => a + b, 0) / values.length;

console.log('--- size per post (bytes on the wire) ---');
const sizes = {
  'shadowhash': [],
  'full post JSON': [],
  'gzip(text) alone': [],
  'gzip(post JSON)': [],
  'raw text': [],
};
for (const { input } of corpus) {
  const json = JSON.stringify(input);
  sizes.shadowhash.push(sh.encodePost(input).length);
  sizes['full post JSON'].push(Buffer.byteLength(json));
  sizes['gzip(text) alone'].push(zlib.gzipSync(Buffer.from(input.text)).length);
  sizes['gzip(post JSON)'].push(zlib.gzipSync(Buffer.from(json)).length);
  sizes['raw text'].push(Buffer.byteLength(input.text));
}
for (const [label, values] of Object.entries(sizes)) {
  console.log(
    `${label.padEnd(20)} mean ${mean(values).toFixed(1).padStart(7)}  p50 ${String(percentile(values, 50)).padStart(5)}  p95 ${String(percentile(values, 95)).padStart(5)}  max ${String(Math.max(...values)).padStart(5)}`
  );
}

const pageEntries = corpus
  .slice(0, 20)
  .map(({ id, input }) => ({ id, hash: sh.encodePost(input) }));
const pageBytes = Buffer.byteLength(JSON.stringify(pageEntries));
console.log(
  `20-post upcoming block (ids + hashes, JSON): ${pageBytes} bytes (${(pageBytes / 20).toFixed(1)}/post)`
);

function bench(label, fn, iterations) {
  for (let index = 0; index < Math.min(iterations, 2000); index++) fn(index);
  const start = process.hrtime.bigint();
  for (let index = 0; index < iterations; index++) fn(index);
  const ns = Number(process.hrtime.bigint() - start);
  console.log(
    `${label.padEnd(36)} ${(1e6 / (ns / iterations)).toFixed(0).padStart(7)}k ops/s   ${(ns / iterations / 1000).toFixed(2).padStart(8)} µs/op`
  );
}

console.log(`\n--- throughput (Node ${process.version}, single core) ---`);
const hashes = corpus.map(({ id, input }) => ({
  id,
  hash: sh.encodePost(input),
}));
bench(
  'encodePost',
  (i) => sh.encodePost(corpus[i % corpus.length].input),
  50000
);
bench(
  'decodePost',
  (i) => sh.decodePost(hashes[i % hashes.length].hash),
  50000
);
bench(
  'decodePost + placeholderText',
  (i) => {
    const shape = sh.decodePost(hashes[i % hashes.length].hash);
    return shape.text ? sh.placeholderText(shape.text) : '';
  },
  50000
);

const first30 = hashes.slice(0, 30);
const store = sh.createHashStore(sh.memoryStorage());
bench(
  'cold start: store+recall+decode 30',
  () => {
    store.remember('feed', first30);
    return store.recall('feed').map(({ hash }) => {
      const shape = sh.decodePost(hash);
      return shape.text ? sh.placeholderText(shape.text) : '';
    });
  },
  5000
);
bench(
  'decode a 20-hash upcoming page',
  () => pageEntries.map(({ hash }) => sh.decodePost(hash)),
  5000
);

const storedValue = JSON.stringify(
  first30.flatMap(({ id, hash }) => [id, hash])
);
console.log(`\n30-row MMKV value: ${Buffer.byteLength(storedValue)} bytes`);
