import { memo, useEffect, useMemo, useState } from 'react';
import {
  View,
  Text,
  ScrollView,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { CheckIcon, CopyIcon } from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import { openUrl } from './openUrl';
import { PulsingDot } from './AssistantTypingIndicator';
import {
  parseMarkdown,
  type MarkdownBlock,
  type MarkdownInline,
} from './markdown';

export interface AssistantMarkdownProps {
  text: string;
  // Shows a trailing pulse after the text.
  streaming?: boolean;
  onCopyCode?: (code: string) => void;
  // Defaults to Linking.openURL for http(s) and mailto links only.
  onOpenLink?: (url: string) => void;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

type Styles = ReturnType<typeof useStyles>;

const COPIED_RESET_MS = 1500;
const TABLE_COLUMN_WIDTH = 140;

const renderInlines = (
  inlines: MarkdownInline[],
  styles: Styles,
  onOpenLink: (url: string) => void
) =>
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
            accessibilityRole={href ? 'link' : undefined}
            onPress={href ? () => onOpenLink(href) : undefined}
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
  labels,
}: {
  language: string;
  code: string;
  closed: boolean;
  onCopyCode?: (code: string) => void;
  labels: AssistantLabels;
}) => {
  const theme = useTheme();
  const styles = useStyles();
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
        <Text style={styles.codeLanguage}>
          {language || labels.codeLanguageFallback}
        </Text>
        {/* Copying half a block is never what anyone wants; wait for the closing fence. */}
        {closed ? (
          <Pressable
            onPress={() => {
              onCopyCode?.(code);
              setCopied(true);
            }}
            hitSlop={8}
            accessibilityRole="button"
            accessibilityLabel={copied ? labels.copied : labels.copyCode}
            style={({ pressed }) => [
              styles.codeCopy,
              pressed && styles.pressed,
            ]}
          >
            {copied ? (
              <CheckIcon
                size={14}
                color={theme.colors.secondaryLabel}
                strokeWidth={1.8}
              />
            ) : (
              <CopyIcon
                size={14}
                color={theme.colors.secondaryLabel}
                strokeWidth={1.3}
              />
            )}
            <Text style={styles.codeCopyText}>
              {copied ? labels.copied : labels.copy}
            </Text>
          </Pressable>
        ) : null}
      </View>
      <ScrollView
        horizontal
        showsHorizontalScrollIndicator={false}
        contentContainerStyle={styles.codeScroll}
      >
        <View>
          {codeLines.map((line, index) => (
            <Text key={index} style={styles.code}>
              {line || ' '}
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
  onOpenLink,
}: {
  header: MarkdownInline[][];
  rows: MarkdownInline[][][];
  onOpenLink: (url: string) => void;
}) => {
  const styles = useStyles();
  return (
    <ScrollView horizontal showsHorizontalScrollIndicator={false}>
      <View style={styles.table}>
        <View style={[styles.tableRow, styles.tableHeaderRow]}>
          {header.map((cell, column) => (
            <View key={column} style={styles.tableCell}>
              <Text style={[styles.tableText, styles.tableHeaderText]}>
                {renderInlines(cell, styles, onOpenLink)}
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
            {header.map((_, column) => (
              <View key={column} style={styles.tableCell}>
                <Text style={styles.tableText}>
                  {renderInlines(row[column] ?? [], styles, onOpenLink)}
                </Text>
              </View>
            ))}
          </View>
        ))}
      </View>
    </ScrollView>
  );
};

/*
 * One block. Memoized on its source text: once a later block exists this one is final,
 * so during a stream only the tail block re-renders per flush.
 */
const MarkdownBlockView = memo(
  ({
    block,
    onCopyCode,
    onOpenLink,
    labels,
  }: {
    block: MarkdownBlock;
    onCopyCode?: (code: string) => void;
    onOpenLink: (url: string) => void;
    labels: AssistantLabels;
  }) => {
    const styles = useStyles();
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
            {renderInlines(block.inlines, styles, onOpenLink)}
          </Text>
        );
      case 'quote':
        return (
          <View style={styles.quote}>
            <Text style={styles.quoteText}>
              {renderInlines(block.inlines, styles, onOpenLink)}
            </Text>
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
                <Text style={styles.paragraph}>
                  {renderInlines(item, styles, onOpenLink)}
                </Text>
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
            labels={labels}
          />
        );
      case 'table':
        return (
          <TableBlock
            header={block.header}
            rows={block.rows}
            onOpenLink={onOpenLink}
          />
        );
      case 'rule':
        return <View style={styles.rule} />;
      default:
        return (
          <Text style={styles.paragraph}>
            {renderInlines(block.inlines, styles, onOpenLink)}
          </Text>
        );
    }
  },
  (prev, next) =>
    prev.block.key === next.block.key &&
    prev.block.raw === next.block.raw &&
    prev.onCopyCode === next.onCopyCode &&
    prev.onOpenLink === next.onOpenLink &&
    prev.labels === next.labels
);

export const AssistantMarkdown = memo(
  ({
    text,
    streaming = false,
    onCopyCode,
    onOpenLink = openUrl,
    labels,
    style,
  }: AssistantMarkdownProps) => {
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    const blocks = useMemo(() => parseMarkdown(text), [text]);

    return (
      <View style={[styles.container, style]}>
        {blocks.map((block) => (
          <MarkdownBlockView
            key={block.key}
            block={block}
            onCopyCode={onCopyCode}
            onOpenLink={onOpenLink}
            labels={l}
          />
        ))}
        {streaming ? <PulsingDot /> : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      gap: theme.spacing.md,
    },
    heading1: {
      color: theme.colors.label,
      ...theme.typography.title2,
    },
    heading2: {
      color: theme.colors.label,
      ...theme.typography.title3,
    },
    heading3: {
      color: theme.colors.label,
      ...theme.typography.headline,
    },
    paragraph: {
      flex: 1,
      color: theme.colors.label,
      ...theme.typography.body,
    },
    bold: {
      fontWeight: theme.fontWeight.semibold,
    },
    italic: {
      fontStyle: 'italic',
    },
    inlineCode: {
      fontFamily: theme.fonts.mono,
      fontSize: theme.fontSize.subhead,
      color: theme.colors.label,
      backgroundColor: theme.colors.fill,
    },
    link: {
      color: theme.colors.accent,
      textDecorationLine: 'underline',
    },
    quote: {
      borderLeftWidth: 3,
      borderLeftColor: theme.colors.separator,
      paddingLeft: theme.spacing.md,
    },
    quoteText: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.body,
    },
    list: {
      gap: theme.spacing.xs,
    },
    listItem: {
      flexDirection: 'row',
    },
    listMarker: {
      width: 22,
      color: theme.colors.secondaryLabel,
      ...theme.typography.body,
    },
    codeBlock: {
      backgroundColor: theme.colors.elevated,
      borderRadius: theme.radius.md,
      overflow: 'hidden',
    },
    codeHeader: {
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'space-between',
      paddingHorizontal: theme.spacing.md,
      paddingVertical: theme.spacing.sm,
      backgroundColor: theme.colors.elevated2,
    },
    codeLanguage: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.caption,
      fontFamily: theme.fonts.mono,
    },
    codeCopy: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
    },
    codeCopyText: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.caption,
      fontWeight: theme.fontWeight.semibold,
    },
    codeScroll: {
      padding: theme.spacing.md,
    },
    code: {
      color: theme.colors.label,
      fontFamily: theme.fonts.mono,
      fontSize: theme.fontSize.footnote,
      lineHeight: 20,
    },
    table: {
      borderWidth: StyleSheet.hairlineWidth,
      borderColor: theme.colors.separator,
      borderRadius: theme.radius.sm,
      overflow: 'hidden',
    },
    tableRow: {
      flexDirection: 'row',
      borderBottomWidth: StyleSheet.hairlineWidth,
      borderBottomColor: theme.colors.separator,
    },
    tableRowLast: {
      borderBottomWidth: 0,
    },
    tableHeaderRow: {
      backgroundColor: theme.colors.elevated,
    },
    tableCell: {
      width: TABLE_COLUMN_WIDTH,
      paddingHorizontal: theme.spacing.md,
      paddingVertical: theme.spacing.sm,
    },
    tableText: {
      color: theme.colors.label,
      ...theme.typography.subhead,
    },
    tableHeaderText: {
      fontWeight: theme.fontWeight.semibold,
    },
    rule: {
      height: StyleSheet.hairlineWidth,
      backgroundColor: theme.colors.separator,
    },
    pressed: {
      opacity: 0.35,
    },
  })
);
