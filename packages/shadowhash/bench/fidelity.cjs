/*
 * Fidelity of the placeholder's height against the real post, laid out by CoreText with the
 * macOS system font in the Feed row's style. Compares the shadowhash placeholder to a chars-only
 * hash and to a fixed three-line skeleton at the feed content widths of real devices. It scores
 * the held-out corpus. `--calibration` scores the corpus LETTERS was tuned on instead. Run
 * `node fidelity.cjs 17 -0.43 22` for the body style; the default is subhead.
 */
const { execFileSync } = require('child_process');
const path = require('path');
const { buildCorpus } = require('./corpus.cjs');
const { buildHeldoutCorpus } = require('./heldout.cjs');
const sh = require('./dist/index.js');

const CALIBRATION = process.argv.includes('--calibration');
const args = process.argv.slice(2).filter((arg) => !arg.startsWith('--'));
const SIZE = Number(args[0] ?? 15);
const KERN = Number(args[1] ?? -0.24);
const LINE_HEIGHT = Number(args[2] ?? 20);
// Feed content width on SE, 393pt and 430pt phones, a tablet column and the web.
const WIDTHS = [291, 309, 346, 600, 700];

const corpus = CALIBRATION
  ? buildCorpus(500).map((post) => ({ ...post, group: 'calibration' }))
  : buildHeldoutCorpus();

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
  const shape = sh.decodeText(hash);
  const truncated = hash !== sh.encodeText(text, { maxNibbles: 1 << 20 });
  meta.set(id, { truncated, letters: shape.kind === 'words' && !truncated });
  /*
   * One unwrapped line of each, for LETTERS' width per character against the real text. Only
   * full word shapes count, where the placeholder has exactly the real text's letters.
   */
  if (meta.get(id).letters) {
    for (const [variant, value] of [
      ['real', text],
      ['hash', placeholder],
    ]) {
      items.push({
        id: `${id}|line|${variant}`,
        text: value.replace(/\s+/g, ' '),
        size: SIZE,
        kern: KERN,
        weight: 'regular',
        width: 1e7,
      });
    }
  }
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
const results = JSON.parse(raw);
const lines = new Map(results.map((result) => [result.id, result.lines]));
const lineWidths = new Map(
  results.map((result) => [result.id, result.lastWidth])
);

function widthRatio(filter) {
  let real = 0;
  let hash = 0;
  for (const { id, group } of corpus) {
    if (!meta.get(id).letters || !filter(id, group)) continue;
    real += lineWidths.get(`${id}|line|real`);
    hash += lineWidths.get(`${id}|line|hash`);
  }
  return real > 0 ? (hash / real).toFixed(4) : '-';
}

const mean = (values) => values.reduce((a, b) => a + b, 0) / values.length;
const percentile = (values, p) => {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[
    Math.min(sorted.length - 1, Math.floor((p / 100) * sorted.length))
  ];
};

function lineDeltas(width, variant, filter) {
  const deltas = [];
  for (const { id, group } of corpus) {
    if (!filter(id, group)) continue;
    const real = lines.get(`${id}|${width}|real`);
    const got =
      variant === 'fixed3' ? 3 : lines.get(`${id}|${width}|${variant}`);
    deltas.push(Math.abs(got - real));
  }
  return deltas;
}

const share = (deltas, most) =>
  `${((deltas.filter((delta) => delta <= most).length / deltas.length) * 100).toFixed(1)}%`;

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
      const deltas = lineDeltas(width, variant, filter);
      if (deltas.length === 0) continue;
      console.log(
        String(width).padEnd(7) +
          variant.padEnd(9) +
          share(deltas, 0).padStart(7) +
          share(deltas, 1).padStart(9) +
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
const groups = [...new Set(corpus.map(({ group }) => group))];
if (groups.length > 1) {
  console.log(`\n=== by group at ${WIDTHS[0]}pt: exact / ±1 line ===`);
  console.log(
    'group'.padEnd(8) +
      'posts'.padStart(6) +
      ['hash', 'chars', 'fixed3'].map((v) => v.padStart(16)).join('') +
      'width'.padStart(9)
  );
  for (const name of groups) {
    const filter = (_id, group) => group === name;
    console.log(
      name.padEnd(8) +
        String(lineDeltas(WIDTHS[0], 'hash', filter).length).padStart(6) +
        ['hash', 'chars', 'fixed3']
          .map((variant) => {
            const deltas = lineDeltas(WIDTHS[0], variant, filter);
            return `${share(deltas, 0)} / ${share(deltas, 1)}`.padStart(16);
          })
          .join('') +
        widthRatio(filter).padStart(9)
    );
  }
}

console.log(
  `\nplaceholder width / real width, unwrapped, over ${[...meta.values()].filter((m) => m.letters).length} full word shapes: ${widthRatio(() => true)}`
);

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
