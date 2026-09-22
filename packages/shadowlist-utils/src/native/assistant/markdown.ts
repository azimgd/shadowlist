/*
 * A small Markdown reader for streamed replies. It covers what chat models write, like
 * headings, lists, quotes, code, tables, rules and four inline styles. It copes with text
 * that stops mid token, since during a stream every render sees an unfinished document.
 *
 * It returns a flat list of blocks. Every block but the last is final, so the renderer
 * memoizes each block on raw and only the last one re-renders per flush.
 */

export interface MarkdownInline {
  style: 'text' | 'bold' | 'italic' | 'code' | 'link';
  text: string;
  href?: string;
}

interface BlockBase {
  key: string;
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
      // False until the closing fence streams in.
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
 * Text after a marker is optional, so a line that is only a marker like # or 1. already
 * gets its final block type. Otherwise it would be a paragraph for one flush and a heading
 * the next, remounting the block and moving everything under it.
 */
const HEADING = /^(#{1,6})(?:\s+(.*))?$/;
const RULE = /^(-{3,}|\*{3,})\s*$/;
const QUOTE = /^>\s?(.*)$/;
/*
 * Allow up to three spaces of indent, so a nested item starts its own block instead of
 * joining the paragraph above. Four or more spaces is indented code, left alone.
 *
 * Bold text at the start of a line must not become a bullet, so the marker needs a space
 * after it or must stand alone.
 */
const BULLET = /^\s{0,3}[-*](?:\s+(.*))?$/;
const ORDERED = /^\s{0,3}(\d+)\.(?:\s+(.*))?$/;
const TABLE_ROW = /^\|/;
/*
 * A row with no cell content. That is the alignment row, any part of it streamed so far,
 * and the bare pipe every row starts as. Matching only the full alignment row would show
 * these as data rows that vanish a token later, so the table would lose a row mid stream.
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
 * Inline styles. An opener with no closer yet runs to the end of the text, so half streamed
 * bold text shows bold right away instead of flashing raw asterisks.
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
     * Keep 2 * 3 and snake_case literal. Italics need a non space after the opener, and an
     * underscore also needs a word boundary before it.
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
     * The label must close right before the opening paren. Searching further ahead would let
     * a stray bracket swallow text up to the next real link, and get slow on bracket heavy lines.
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

/*
 * Row cells without the outer pipes. A row still streaming in just has fewer cells.
 */
const splitTableRow = (line: string) =>
  line
    .trim()
    .replace(/^\|/, '')
    .replace(/\|$/, '')
    .split('|')
    .map((cell) => parseInline(cell.trim()));

export function parseMarkdown(source: string): MarkdownBlock[] {
  // Split on Windows line endings too, or every line keeps a stray carriage return and matches nothing.
  const lines = source.split(/\r?\n/);
  /*
   * Drop an opening fence that is still streaming in. Left in, it joins the paragraph above
   * for one flush and leaves when the third backtick lands, moving everything under it.
   * The closing fence gets the same treatment below.
   */
  if (PARTIAL_FENCE.test(lines[lines.length - 1] ?? '')) {
    lines.pop();
  }
  const blocks: MarkdownBlock[] = [];
  const keyFor = (type: MarkdownBlock['type']) => `${blocks.length}:${type}`;
  let index = 0;

  /*
   * Take the next lines that match pattern and return each line's first capture.
   */
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
       * While the block is open, drop trailing blank lines and a half streamed closing fence.
       * Otherwise the block grows a line or two and shrinks when the fence completes, which
       * moves everything below and the scroll offset. Once closed, blank lines are kept.
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
