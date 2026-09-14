import { memo, useEffect, useMemo, useState } from 'react';
import {
  View,
  Text,
  ScrollView,
  Pressable,
  Linking,
  StyleSheet,
} from 'react-native';
import {
  colors,
  typography,
  spacing,
  radius,
  fontSize,
  fontWeight,
  MONO_FONT_FAMILY,
} from '../theme';
import { Check, Copy } from '../icons';
import { PulsingDot } from './AssistantTypingIndicator';
import {
  parseMarkdown,
  type MarkdownBlock,
  type MarkdownInline,
} from './markdown';

export interface AssistantMarkdownProps {
  text: string;
  // True while the text is still arriving: a cursor trails the last block.
  streaming?: boolean;
  onCopyCode?: (code: string) => void;
}

// How long a code block's button reads "Copied" before reverting.
const COPIED_RESET_MS = 1500;
// Fixed column width keeps cells aligned across rows without measuring every cell first.
const TABLE_COLUMN_WIDTH = 140;

const openLink = (href: string) => {
  Linking.openURL(href).catch(() => {});
};

const renderInlines = (inlines: MarkdownInline[]) =>
  inlines.map((inline, index) => {
    switch (inline.style) {
      case 'bold':
        return (
          <Text key={index} style={styles.bold}>
            {inline.text}
          </Text>
        );
      case 'italic':
        return (
          <Text key={index} style={styles.italic}>
            {inline.text}
          </Text>
        );
      case 'code':
        return (
          <Text key={index} style={styles.inlineCode}>
            {inline.text}
          </Text>
        );
      case 'link': {
        const href = inline.href;
        return (
          <Text
            key={index}
            style={styles.link}
            onPress={href ? () => openLink(href) : undefined}
          >
            {inline.text}
          </Text>
        );
      }
      default:
        return <Text key={index}>{inline.text}</Text>;
    }
  });

const CodeBlock = ({
  language,
  code,
  closed,
  onCopyCode,
}: {
  language: string;
  code: string;
  closed: boolean;
  onCopyCode?: (code: string) => void;
}) => {
  const [copied, setCopied] = useState(false);
  /*
   * One <Text> per line rather than one <Text> for the block. A fence that is still
   * streaming appends to its LAST line, and with a single text run that re-lays-out and
   * re-paints every line above it on each flush; split, only the last line is dirty.
   * Empty lines carry a non-breaking space so they keep their line box.
   */
  const codeLines = useMemo(() => code.split('\n'), [code]);

  useEffect(() => {
    if (!copied) return;
    const id = setTimeout(() => setCopied(false), COPIED_RESET_MS);
    return () => clearTimeout(id);
  }, [copied]);

  return (
    <View style={styles.codeBlock}>
      <View style={styles.codeHeader}>
        <Text style={styles.codeLanguage}>{language || 'code'}</Text>
        {/* Copying half a block is never what anyone wants; wait for the closing fence. */}
        {closed ? (
          <Pressable
            onPress={() => {
              onCopyCode?.(code);
              setCopied(true);
            }}
            hitSlop={8}
            accessibilityRole="button"
            accessibilityLabel="Copy code"
            style={({ pressed }) => [
              styles.codeCopy,
              pressed && styles.pressed,
            ]}
          >
            {copied ? (
              <Check
                size={14}
                color={colors.secondaryLabel}
                strokeWidth={1.8}
              />
            ) : (
              <Copy size={14} color={colors.secondaryLabel} strokeWidth={1.3} />
            )}
            <Text style={styles.codeCopyText}>
              {copied ? 'Copied' : 'Copy'}
            </Text>
          </Pressable>
        ) : null}
      </View>
      <ScrollView
        horizontal
        showsHorizontalScrollIndicator={false}
        contentContainerStyle={styles.codeScroll}
      >
        {/* The horizontal content container lays out in a row; the lines stack in here. */}
        <View>
          {codeLines.map((line, index) => (
            <Text key={index} style={styles.code}>
              {line || '\u00a0'}
            </Text>
          ))}
        </View>
      </ScrollView>
    </View>
  );
};

const TableBlock = ({
  header,
  rows,
}: {
  header: MarkdownInline[][];
  rows: MarkdownInline[][][];
}) => (
  <ScrollView horizontal showsHorizontalScrollIndicator={false}>
    <View style={styles.table}>
      <View style={[styles.tableRow, styles.tableHeaderRow]}>
        {header.map((cell, column) => (
          <View key={column} style={styles.tableCell}>
            <Text style={[styles.tableText, styles.tableHeaderText]}>
              {renderInlines(cell)}
            </Text>
          </View>
        ))}
      </View>
      {rows.map((row, rowIndex) => (
        <View
          key={rowIndex}
          style={[
            styles.tableRow,
            rowIndex === rows.length - 1 && styles.tableRowLast,
          ]}
        >
          {/* Driven by the header so a row still streaming in keeps its column slots. */}
          {header.map((_, column) => (
            <View key={column} style={styles.tableCell}>
              <Text style={styles.tableText}>
                {renderInlines(row[column] ?? [])}
              </Text>
            </View>
          ))}
        </View>
      ))}
    </View>
  </ScrollView>
);

/*
 * One block. Memoized on its source text: once a later block exists this one is final,
 * so during a stream only the tail block re-renders per flush.
 */
const MarkdownBlockView = memo(
  ({
    block,
    onCopyCode,
  }: {
    block: MarkdownBlock;
    onCopyCode?: (code: string) => void;
  }) => {
    switch (block.type) {
      case 'heading':
        return (
          <Text
            style={
              block.level === 1
                ? styles.heading1
                : block.level === 2
                  ? styles.heading2
                  : styles.heading3
            }
            accessibilityRole="header"
          >
            {renderInlines(block.inlines)}
          </Text>
        );
      case 'quote':
        return (
          <View style={styles.quote}>
            <Text style={styles.quoteText}>{renderInlines(block.inlines)}</Text>
          </View>
        );
      case 'list':
        return (
          <View style={styles.list}>
            {block.items.map((item, index) => (
              <View key={index} style={styles.listItem}>
                <Text style={styles.listMarker}>
                  {block.ordered ? `${block.start + index}.` : '•'}
                </Text>
                <Text style={styles.paragraph}>{renderInlines(item)}</Text>
              </View>
            ))}
          </View>
        );
      case 'code':
        return (
          <CodeBlock
            language={block.language}
            code={block.code}
            closed={block.closed}
            onCopyCode={onCopyCode}
          />
        );
      case 'table':
        return <TableBlock header={block.header} rows={block.rows} />;
      case 'rule':
        return <View style={styles.rule} />;
      default:
        return (
          <Text style={styles.paragraph}>{renderInlines(block.inlines)}</Text>
        );
    }
  },
  (prev, next) =>
    prev.block.key === next.block.key &&
    prev.block.raw === next.block.raw &&
    prev.onCopyCode === next.onCopyCode
);

// Renders a (possibly still streaming) Markdown reply as native text, code and tables.
export const AssistantMarkdown = memo(
  ({ text, streaming = false, onCopyCode }: AssistantMarkdownProps) => {
    const blocks = useMemo(() => parseMarkdown(text), [text]);

    return (
      <View style={styles.container}>
        {blocks.map((block) => (
          <MarkdownBlockView
            key={block.key}
            block={block}
            onCopyCode={onCopyCode}
          />
        ))}
        {streaming ? <PulsingDot /> : null}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    gap: spacing.md,
  },
  heading1: {
    color: colors.label,
    ...typography.title2,
  },
  heading2: {
    color: colors.label,
    ...typography.title3,
  },
  heading3: {
    color: colors.label,
    ...typography.headline,
  },
  paragraph: {
    flex: 1,
    color: colors.label,
    ...typography.body,
  },
  bold: {
    fontWeight: fontWeight.semibold,
  },
  italic: {
    fontStyle: 'italic',
  },
  inlineCode: {
    fontFamily: MONO_FONT_FAMILY,
    fontSize: fontSize.subhead,
    color: colors.label,
    backgroundColor: colors.fill,
  },
  link: {
    color: colors.accent,
    textDecorationLine: 'underline',
  },
  quote: {
    borderLeftWidth: 3,
    borderLeftColor: colors.separator,
    paddingLeft: spacing.md,
  },
  quoteText: {
    color: colors.secondaryLabel,
    ...typography.body,
  },
  list: {
    gap: spacing.xs,
  },
  listItem: {
    flexDirection: 'row',
  },
  listMarker: {
    width: 22,
    color: colors.secondaryLabel,
    ...typography.body,
  },
  codeBlock: {
    backgroundColor: colors.elevated,
    borderRadius: radius.md,
    overflow: 'hidden',
  },
  codeHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.sm,
    backgroundColor: colors.elevated2,
  },
  codeLanguage: {
    color: colors.secondaryLabel,
    ...typography.caption,
    fontFamily: MONO_FONT_FAMILY,
  },
  codeCopy: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.xs,
  },
  codeCopyText: {
    color: colors.secondaryLabel,
    ...typography.caption,
    fontWeight: fontWeight.semibold,
  },
  codeScroll: {
    padding: spacing.md,
  },
  code: {
    color: colors.label,
    fontFamily: MONO_FONT_FAMILY,
    fontSize: fontSize.footnote,
    lineHeight: 20,
  },
  table: {
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: colors.separator,
    borderRadius: radius.sm,
    overflow: 'hidden',
  },
  tableRow: {
    flexDirection: 'row',
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: colors.separator,
  },
  tableRowLast: {
    borderBottomWidth: 0,
  },
  tableHeaderRow: {
    backgroundColor: colors.elevated,
  },
  tableCell: {
    width: TABLE_COLUMN_WIDTH,
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.sm,
  },
  tableText: {
    color: colors.label,
    ...typography.subhead,
  },
  tableHeaderText: {
    fontWeight: fontWeight.semibold,
  },
  rule: {
    height: StyleSheet.hairlineWidth,
    backgroundColor: colors.separator,
  },
  pressed: {
    opacity: 0.35,
  },
});
