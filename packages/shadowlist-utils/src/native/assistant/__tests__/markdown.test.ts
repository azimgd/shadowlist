import { parseMarkdown } from '../markdown';

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
});
