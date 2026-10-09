import { View, Text, ScrollView, StyleSheet } from 'react-native';
import { createStyles } from '../theme';
import { renderInlines, useInlineStyles } from './AssistantMarkdownInlines';
import type { MarkdownInline } from './markdown';

export interface AssistantMarkdownTableProps {
  header: MarkdownInline[][];
  rows: MarkdownInline[][][];
  onOpenLink: (url: string) => void;
}

const TABLE_COLUMN_WIDTH = 140;

export const AssistantMarkdownTable = ({
  header,
  rows,
  onOpenLink,
}: AssistantMarkdownTableProps) => {
  const styles = useStyles();
  const inlineStyles = useInlineStyles();
  return (
    <ScrollView horizontal showsHorizontalScrollIndicator={false}>
      <View style={styles.table}>
        <View style={[styles.tableRow, styles.tableHeaderRow]}>
          {header.map((cell, column) => (
            <View key={column} style={styles.tableCell}>
              <Text style={[styles.tableText, styles.tableHeaderText]}>
                {renderInlines(cell, inlineStyles, onOpenLink)}
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
                  {renderInlines(row[column] ?? [], inlineStyles, onOpenLink)}
                </Text>
              </View>
            ))}
          </View>
        ))}
      </View>
    </ScrollView>
  );
};

const useStyles = createStyles((theme) =>
  StyleSheet.create({
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
  })
);
