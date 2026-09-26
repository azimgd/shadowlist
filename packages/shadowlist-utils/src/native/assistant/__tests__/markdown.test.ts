import {
  parseMarkdown,
  parseMarkdownFrom,
  type MarkdownParse,
} from '../markdown';

describe('parseMarkdown', () => {
  it('parses headings, paragraphs and lists', () => {
    const blocks = parseMarkdown(
      '# Title\n\nSome **bold** text\n\n- one\n- two'
    );
    expect(blocks.map((block) => block.type)).toEqual([
      'heading',
      'paragraph',
      'list',
    ]);
  });

  it('marks an unterminated code fence as open while streaming', () => {
    const [open] = parseMarkdown('```ts\nconst a = 1;');
    expect(open).toMatchObject({ type: 'code', language: 'ts', closed: false });

    const [closed] = parseMarkdown('```ts\nconst a = 1;\n```');
    expect(closed).toMatchObject({
      type: 'code',
      closed: true,
      code: 'const a = 1;',
    });
  });

  it('extracts link hrefs', () => {
    const [paragraph] = parseMarkdown('See [docs](https://example.com) now');
    expect(paragraph?.type).toBe('paragraph');
    const inlines = paragraph?.type === 'paragraph' ? paragraph.inlines : [];
    expect(inlines).toContainEqual(
      expect.objectContaining({ style: 'link', href: 'https://example.com' })
    );
  });

  it('keeps arithmetic and snake_case literal', () => {
    const [paragraph] = parseMarkdown('2 * 3 and snake_case_name');
    const inlines = paragraph?.type === 'paragraph' ? paragraph.inlines : [];
    expect(inlines.every((inline) => inline.style === 'text')).toBe(true);
  });

  it('does not throw on partial input at every prefix', () => {
    const source =
      '# Plan\n\n1. **Book** [flight](https://x.io)\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```js\nx\n```\n> quote';
    for (let end = 0; end <= source.length; end++) {
      expect(() => parseMarkdown(source.slice(0, end))).not.toThrow();
    }
  });

  it('continues a streamed parse with the same blocks as a full parse', () => {
    const documents = [
      '# Plan\n\n1. **Book** [flight](https://x.io)\n2. Pack\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```js\nx\n\n```\n> quote\n> more\n\n---\nText *it* `code`\n- a\n- b\n# Next\npara\n#\n##x\n',
      'para one\r\nstill one\r\n\r\n- item\r\n  - nested\r\n```\r\ncode\r\n``\r\n```\r\nafter',
      '```\nopen fence never closed\n\n\n',
      'a\n#\n#b\n# c\n***\n**bold** start\n|x|\n|-|\n',
    ];
    for (const source of documents) {
      for (const step of [1, 2, 3, 7]) {
        let parse: MarkdownParse | null = null;
        for (let end = 0; end <= source.length; end += step) {
          const text = source.slice(0, end);
          parse = parseMarkdownFrom(parse, text);
          expect(parse.blocks).toEqual(parseMarkdown(text));
        }
        parse = parseMarkdownFrom(parse, source);
        expect(parse.blocks).toEqual(parseMarkdown(source));
      }
    }
  });

  it('keeps finished blocks as the same objects while streaming', () => {
    const first = parseMarkdownFrom(null, '# Title\n\nfirst para\n\nsec');
    const next = parseMarkdownFrom(first, '# Title\n\nfirst para\n\nsecond');
    expect(next.blocks[0]).toBe(first.blocks[0]);
    expect(next.blocks[1]).toBe(first.blocks[1]);
    // Text that isn't a continuation is parsed from scratch.
    const other = parseMarkdownFrom(next, '# Other');
    expect(other.blocks).toEqual(parseMarkdown('# Other'));
  });
});
