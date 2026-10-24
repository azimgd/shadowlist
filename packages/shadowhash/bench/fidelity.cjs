/*
 * Fidelity of the placeholder's height against the real post, laid out by CoreText, the engine
 * behind React Native's iOS Text, in the Feed row's style. Compares the shadowhash placeholder
 * to a chars-only hash and to a fixed three-line skeleton at the feed content widths of real
 * devices. Run `node fidelity.cjs 17 -0.43 22` for the body style; the default is subhead.
 */
const { execFileSync } = require('child_process');
const path = require('path');
const { buildCorpus } = require('./corpus.cjs');
const sh = require('./dist/index.js');

const SIZE = Number(process.argv[2] ?? 15);
const KERN = Number(process.argv[3] ?? -0.24);
const LINE_HEIGHT = Number(process.argv[4] ?? 20);
// Feed content width on SE, 393pt and 430pt phones, a tablet column and the web.
const WIDTHS = [291, 309, 346, 600, 700];

const corpus = buildCorpus(500);

/*
 * The chars-only baseline: all that survives is the character count, filled back as five-letter
 * words. It stands for a format that stores one length instead of the text's shape.
 */
function charsOnlyPlaceholder(text) {
  const count = [...text.replace(/\s+/g, '')].length;
  const tokens = [];
  let rest = count;
  while (rest > 0) {
    tokens.push(Math.min(5, rest));
    rest -= 5;
  }
  return sh.placeholderText({ kind: 'words', tokens });
}

const items = [];
const meta = new Map();
for (const { id, text } of corpus) {
  const hash = sh.encodeText(text);
  const placeholder = sh.placeholderText(sh.decodeText(hash));
  meta.set(id, {
    truncated: hash !== sh.encodeText(text, { maxNibbles: 1 << 20 }),
  });
  for (const width of WIDTHS) {
    for (const [variant, value] of [
      ['real', text],
      ['hash', placeholder],
      ['chars', charsOnlyPlaceholder(text)],
    ]) {
      items.push({
        id: `${id}|${width}|${variant}`,
        text: value,
        size: SIZE,
        kern: KERN,
        weight: 'regular',
        width,
      });
    }
  }
}

const raw = execFileSync(path.join(__dirname, 'layout'), {
  input: JSON.stringify({ items }),
  maxBuffer: 256 * 1024 * 1024,
});
const lines = new Map(
  JSON.parse(raw).map((result) => [result.id, result.lines])
);

const mean = (values) => values.reduce((a, b) => a + b, 0) / values.length;
const percentile = (values, p) => {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[
    Math.min(sorted.length - 1, Math.floor((p / 100) * sorted.length))
  ];
};

function report(label, filter) {
  console.log(`\n=== ${label} ===`);
  console.log(
    'width'.padEnd(7) +
      'variant'.padEnd(9) +
      'exact'.padStart(7) +
      '±1 line'.padStart(9) +
      'mean|Δh|px'.padStart(12) +
      'p95|Δh|px'.padStart(11) +
      'max|Δl|'.padStart(9)
  );
  for (const width of WIDTHS) {
    for (const variant of ['hash', 'chars', 'fixed3']) {
      const deltas = [];
      for (const { id } of corpus) {
        if (!filter(id)) continue;
        const real = lines.get(`${id}|${width}|real`);
        const got =
          variant === 'fixed3' ? 3 : lines.get(`${id}|${width}|${variant}`);
        deltas.push(Math.abs(got - real));
      }
      if (deltas.length === 0) continue;
      const exact =
        deltas.filter((delta) => delta === 0).length / deltas.length;
      const near = deltas.filter((delta) => delta <= 1).length / deltas.length;
      console.log(
        String(width).padEnd(7) +
          variant.padEnd(9) +
          `${(exact * 100).toFixed(1)}%`.padStart(7) +
          `${(near * 100).toFixed(1)}%`.padStart(9) +
          (mean(deltas) * LINE_HEIGHT).toFixed(1).padStart(12) +
          (percentile(deltas, 95) * LINE_HEIGHT).toFixed(0).padStart(11) +
          String(Math.max(...deltas)).padStart(9)
      );
    }
  }
}

report(`all ${corpus.length} posts`, () => true);
report('posts within 64 nibbles', (id) => !meta.get(id).truncated);
report('truncated posts', (id) => meta.get(id).truncated);
const truncatedCount = [...meta.values()].filter((m) => m.truncated).length;
console.log(`\ntruncated: ${truncatedCount}/${corpus.length} posts`);

for (const variant of ['hash', 'chars', 'fixed3']) {
  let shift = 0;
  for (const { id } of corpus) {
    const real = lines.get(`${id}|309|real`);
    const got = variant === 'fixed3' ? 3 : lines.get(`${id}|309|${variant}`);
    shift += Math.abs(got - real) * LINE_HEIGHT;
  }
  console.log(
    `${variant.padEnd(7)} mean text-height shift per row at 309pt: ${(shift / corpus.length).toFixed(1)}px (~${((shift / corpus.length) * 8).toFixed(0)}px per 8-row screen)`
  );
}
