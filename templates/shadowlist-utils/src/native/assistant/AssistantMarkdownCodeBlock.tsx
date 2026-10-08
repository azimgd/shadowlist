import { useEffect, useMemo, useState } from 'react';
import { View, Text, ScrollView, Pressable, StyleSheet } from 'react-native';
import { createStyles, useTheme } from '../theme';
import { CheckIcon, CopyIcon } from '../icons';
import type { AssistantLabels } from './labels';

export interface AssistantMarkdownCodeBlockProps {
  language: string;
  code: string;
  closed: boolean;
  onCopyCode?: (code: string) => void;
  labels: AssistantLabels;
}

const COPIED_RESET_MS = 1500;

export const AssistantMarkdownCodeBlock = ({
  language,
  code,
  closed,
  onCopyCode,
  labels,
}: AssistantMarkdownCodeBlockProps) => {
  const theme = useTheme();
  const styles = useStyles();
  const [copied, setCopied] = useState(false);
  /*
   * One Text per line instead of one for the block. Streaming code only grows its last
   * line. Only that line redraws on each flush. Empty lines hold a non breaking space
   * so they keep their height.
   */
  const codeLines = useMemo(() => code.split('\n'), [code]);

  useEffect(() => {
    if (!copied) {
      return;
    }
    const id = setTimeout(() => setCopied(false), COPIED_RESET_MS);
    return () => clearTimeout(id);
  }, [copied]);

  return (
    <View style={styles.codeBlock}>
      <View style={styles.codeHeader}>
        <Text style={styles.codeLanguage}>
          {language || labels.codeLanguageFallback}
        </Text>
        {/* Nobody wants half a block. Wait for the closing fence. */}
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
              {line || ' '}
            </Text>
          ))}
        </View>
      </ScrollView>
    </View>
  );
};

const useStyles = createStyles((theme) =>
  StyleSheet.create({
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
    pressed: {
      opacity: 0.35,
    },
  })
);
