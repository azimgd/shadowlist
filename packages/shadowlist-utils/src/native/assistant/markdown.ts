/*
 * A deliberately small Markdown reader for streamed replies. It covers the subset chat
 * models actually emit -- headings, paragraphs, lists, quotes, fenced code, tables, rules
 * and four inline styles -- and it tolerates text that stops mid-token, because during a
 * stream every render sees an unfinished document.
 *
 * Output is a flat list of blocks. Every block but the last is final once a later block
 * exists, so the renderer memoizes each block on `raw` and only the tail re-renders per
 * flush: block memoization, applied to the parse output rather than to the parse.
 */

export interface MarkdownInline {
  style: 'text' | 'bold' | 'italic' | 'code' | 'link';
  text: string;
  href?: string;
}

interface BlockBase {
  // Position plus type, so a tail that turns from a paragraph into a list remounts.
  key: string;
  // Source text of the block; the memo comparison for everything the block renders.
  raw: string;
}

export type MarkdownBlock =
  | (BlockBase & { type: 'heading'; level: number; inlines: MarkdownInline[] })
  | (BlockBase & { type: 'paragraph'; inlines: MarkdownInline[] })
  | (BlockBase & { type: 'quote'; inlines: MarkdownInline[] })
  | (BlockBase & {
      type: 'list';
      ordered: boolean;
      start: number;
      items: MarkdownInline[][];
    })
  | (BlockBase & {
      type: 'code';
      language: string;
      code: string;
      // False while the closing fence has not streamed in yet.
      closed: boolean;
    })
  | (BlockBase & {
      type: 'table';
      header: MarkdownInline[][];
      rows: MarkdownInline[][][];
    })
  | (BlockBase & { type: 'rule' });

const FENCE_OPEN = /^```(\S*)\s*$/;
const FENCE_CLOSE = /^```\s*$/;
const PARTIAL_FENCE = /^`{1,2}\s*$/;
/*
 * The trailing text of a marker is optional throughout, so a line that is still only its
 * marker (`#`, `-`, `1.`) already settles on its final block type. Without that the line
 * is a paragraph for one flush and a heading or list item on the next, which remounts the
 * block and reflows everything under it -- once per marker, in a stream full of them.
 */
const HEADING = /^(#{1,6})(?:\s+(.*))?$/;
const RULE = /^(-{3,}|\*{3,})\s*$/;
const QUOTE = /^>\s?(.*)$/;
/*
 * Up to three spaces of indent, so a nested item is its own block rather than a
 * continuation line glued onto the paragraph above. Rendering stays flat; this is about
 * where blocks begin, not about nesting. Four or more spaces is indented code, left alone.
 *
 * `**bold**` at the start of a line must not become a bullet: the optional tail requires
 * whitespace after the marker, so only `* ` (or a lone `*`) opens a list.
 */
const BULLET = /^\s{0,3}[-*](?:\s+(.*))?$/;
const ORDERED = /^\s{0,3}(\d+)\.(?:\s+(.*))?$/;
const TABLE_ROW = /^\|/;
/*
 * A row carrying no cell content: the alignment row (`| --- | --- |`), any half of one a
 * stream has delivered so far (`| --- | ---`, before its final pipe), and the bare `|` that
 * every row starts life as. All of it is structure, never data. Matching only the complete
 * alignment row renders each of these as a data row that then vanishes a token later, so
 * the table visibly loses a row mid-stream.
 */
const TABLE_STRUCTURE_ROW = /^[\s:|-]*$/;

const isBlockStart = (line: string) =>
  FENCE_OPEN.test(line) ||
  HEADING.test(line) ||
  RULE.test(line) ||
  QUOTE.test(line) ||
  BULLET.test(line) ||
  ORDERED.test(line) ||
  TABLE_ROW.test(line);

const WORD_CHAR = /\w/;

/*
 * Inline styles. An opener with no closer yet runs to the end of the text, so mid-stream
 * `**bol` renders bold straight away instead of flashing literal asterisks until the
 * closer arrives -- the same termination rule streaming Markdown renderers apply.
 */
function parseInline(text: string): MarkdownInline[] {
  const inlines: MarkdownInline[] = [];
  let plain = '';
  let index = 0;

  const flushPlain = () => {
    if (plain) {
      inlines.push({ style: 'text', text: plain });
      plain = '';
    }
  };

  const readUntil = (marker: string, from: number) => {
    const end = text.indexOf(marker, from);
    return end === -1
      ? { body: text.slice(from), next: text.length }
      : { body: text.slice(from, end), next: end + marker.length };
  };

  while (index < text.length) {
    const char = text.charAt(index);
    const nextChar = text.charAt(index + 1);

    if (char === '`') {
      flushPlain();
      const { body, next } = readUntil('`', index + 1);
      if (body) inlines.push({ style: 'code', text: body });
      index = next;
      continue;
    }

    if (char === '*' && nextChar === '*') {
      flushPlain();
      const { body, next } = readUntil('**', index + 2);
      if (body) inlines.push({ style: 'bold', text: body });
      index = next;
      continue;
    }

    /*
     * `2 * 3` and snake_case stay literal: italics need a non-space after the opener and,
     * for underscores, a word boundary before it.
     */
    const opensItalic =
      (char === '*' || char === '_') &&
      nextChar !== '' &&
      nextChar !== ' ' &&
      !(char === '_' && WORD_CHAR.test(text.charAt(index - 1)));
    if (opensItalic) {
      flushPlain();
      const { body, next } = readUntil(char, index + 1);
      if (body) inlines.push({ style: 'italic', text: body });
      index = next;
      continue;
    }

    /*
     * The label's closing `]` has to be the one immediately before the `(`. Searching for
     * the first `](` anywhere ahead instead lets a stray bracket in prose swallow every
     * character up to the next real link, and makes a line full of brackets quadratic.
     */
    if (char === '[') {
      const labelEnd = text.indexOf(']', index + 1);
      if (labelEnd !== -1 && text.charAt(labelEnd + 1) === '(') {
        flushPlain();
        const urlEnd = text.indexOf(')', labelEnd + 2);
        inlines.push({
          style: 'link',
          text: text.slice(index + 1, labelEnd),
          href: urlEnd === -1 ? undefined : text.slice(labelEnd + 2, urlEnd),
        });
        index = urlEnd === -1 ? text.length : urlEnd + 1;
        continue;
      }
    }

    plain += char;
    index += 1;
  }

  flushPlain();
  return inlines;
}

// Row cells without the outer pipes. A row still streaming in simply has fewer cells.
const splitTableRow = (line: string) =>
  line
    .trim()
    .replace(/^\|/, '')
    .replace(/\|$/, '')
    .split('|')
    .map((cell) => parseInline(cell.trim()));

export function parseMarkdown(source: string): MarkdownBlock[] {
  // CRLF sources would otherwise leave a stray \r on every line and match no block pattern.
  const lines = source.split(/\r?\n/);
  /*
   * An OPENING fence still streaming in (` or ``) is not text. Left in place it is absorbed
   * into the paragraph above for one flush and pulled back out when the third backtick
   * lands, reflowing that paragraph and everything under it. The mirror of the closing
   * fence pop below.
   */
  if (PARTIAL_FENCE.test(lines[lines.length - 1] ?? '')) {
    lines.pop();
  }
  const blocks: MarkdownBlock[] = [];
  const keyFor = (type: MarkdownBlock['type']) => `${blocks.length}:${type}`;
  let index = 0;

  // Consume consecutive lines matching `pattern`, returning each line's first capture.
  const collect = (pattern: RegExp) => {
    const start = index;
    const captures: RegExpExecArray[] = [];
    let match = pattern.exec(lines[index] ?? '');
    while (index < lines.length && match) {
      captures.push(match);
      index += 1;
      match = pattern.exec(lines[index] ?? '');
    }
    return { captures, raw: lines.slice(start, index).join('\n') };
  };

  while (index < lines.length) {
    const line = lines[index] ?? '';

    if (!line.trim()) {
      index += 1;
      continue;
    }

    const fence = FENCE_OPEN.exec(line);
    if (fence) {
      const start = index;
      const body: string[] = [];
      index += 1;
      while (index < lines.length && !FENCE_CLOSE.test(lines[index] ?? '')) {
        body.push(lines[index] ?? '');
        index += 1;
      }
      const closed = index < lines.length;
      if (closed) index += 1;
      /*
       * While the block is still open, drop trailing blank lines and a closing fence still
       * streaming in (` or ``). Neither is code: the newline before the fence arrives as an
       * empty body line, and the backticks arrive as another, so the block grows by a line
       * or two and shrinks again when the fence completes -- which moves everything below
       * it and, at the bottom of a list, the scroll offset. Once closed, blank lines inside
       * the fence are real content and are kept.
       */
      while (!closed && body.length > 0) {
        const tail = body[body.length - 1] ?? '';
        if (!tail.trim() || PARTIAL_FENCE.test(tail)) {
          body.pop();
          continue;
        }
        break;
      }
      blocks.push({
        key: keyFor('code'),
        type: 'code',
        raw: lines.slice(start, index).join('\n'),
        language: fence[1] ?? '',
        code: body.join('\n'),
        closed,
      });
      continue;
    }

    const heading = HEADING.exec(line);
    if (heading) {
      blocks.push({
        key: keyFor('heading'),
        type: 'heading',
        raw: line,
        level: (heading[1] ?? '#').length,
        inlines: parseInline(heading[2] ?? ''),
      });
      index += 1;
      continue;
    }

    if (RULE.test(line)) {
      blocks.push({ key: keyFor('rule'), type: 'rule', raw: line });
      index += 1;
      continue;
    }

    if (QUOTE.test(line)) {
      const { captures, raw } = collect(QUOTE);
      blocks.push({
        key: keyFor('quote'),
        type: 'quote',
        raw,
        inlines: parseInline(captures.map((match) => match[1]).join(' ')),
      });
      continue;
    }

    const ordered = ORDERED.test(line);
    if (ordered || BULLET.test(line)) {
      const pattern = ordered ? ORDERED : BULLET;
      const { captures, raw } = collect(pattern);
      blocks.push({
        key: keyFor('list'),
        type: 'list',
        raw,
        ordered,
        start: ordered ? Number(captures[0]?.[1] ?? 1) : 1,
        items: captures.map((match) =>
          parseInline((ordered ? match[2] : match[1]) ?? '')
        ),
      });
      continue;
    }

    if (TABLE_ROW.test(line)) {
      const { captures, raw } = collect(/^(\|.*)$/);
      const rowLines = captures
        .map((match) => match[1] ?? '')
        .filter((rowLine) => !TABLE_STRUCTURE_ROW.test(rowLine));
      blocks.push({
        key: keyFor('table'),
        type: 'table',
        raw,
        header: splitTableRow(rowLines[0] ?? ''),
        rows: rowLines.slice(1).map(splitTableRow),
      });
      continue;
    }

    const start = index;
    index += 1;
    while (
      index < lines.length &&
      (lines[index] ?? '').trim() &&
      !isBlockStart(lines[index] ?? '')
    ) {
      index += 1;
    }
    const raw = lines.slice(start, index).join('\n');
    blocks.push({
      key: keyFor('paragraph'),
      type: 'paragraph',
      raw,
      inlines: parseInline(raw),
    });
  }

  return blocks;
}
