import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { createStyles } from '../theme';
import type { AssistantLabels } from './labels';
import { AssistantMarkdownCodeBlock } from './AssistantMarkdownCodeBlock';
import { renderInlines, useInlineStyles } from './AssistantMarkdownInlines';
import { AssistantMarkdownTable } from './AssistantMarkdownTable';
import type { MarkdownBlock } from './markdown';

export interface AssistantMarkdownBlockProps {
  block: MarkdownBlock;
  onCopyCode?: (code: string) => void;
  onOpenLink: (url: string) => void;
  labels: AssistantLabels;
}

/*
 * One block, memoized on its source text. Once a later block exists this one is final.
 * Only the last block re-renders per flush.
 */
export const AssistantMarkdownBlock = memo(
  ({ block, onCopyCode, onOpenLink, labels }: AssistantMarkdownBlockProps) => {
    const styles = useStyles();
    const inlineStyles = useInlineStyles();
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
            {renderInlines(block.inlines, inlineStyles, onOpenLink)}
          </Text>
        );
      case 'quote':
        return (
          <View style={styles.quote}>
            <Text style={styles.quoteText}>
              {renderInlines(block.inlines, inlineStyles, onOpenLink)}
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
                  {renderInlines(item, inlineStyles, onOpenLink)}
                </Text>
              </View>
            ))}
          </View>
        );
      case 'code':
        return (
          <AssistantMarkdownCodeBlock
            language={block.language}
            code={block.code}
            closed={block.closed}
            onCopyCode={onCopyCode}
            labels={labels}
          />
        );
      case 'table':
        return (
          <AssistantMarkdownTable
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
            {renderInlines(block.inlines, inlineStyles, onOpenLink)}
          </Text>
        );
    }
  },
  (previous, next) =>
    previous.block.key === next.block.key &&
    previous.block.raw === next.block.raw &&
    previous.onCopyCode === next.onCopyCode &&
    previous.onOpenLink === next.onOpenLink &&
    previous.labels === next.labels
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
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
    rule: {
      height: StyleSheet.hairlineWidth,
      backgroundColor: theme.colors.separator,
    },
  })
);
