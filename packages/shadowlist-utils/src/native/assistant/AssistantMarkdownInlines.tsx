import { StyleSheet, Text } from 'react-native';
import { createStyles } from '../theme';
import type { MarkdownInline } from './markdown';

export type InlineStyles = ReturnType<typeof useInlineStyles>;

export const renderInlines = (
  inlines: MarkdownInline[],
  styles: InlineStyles,
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

export const useInlineStyles = createStyles((theme) =>
  StyleSheet.create({
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
  })
);
