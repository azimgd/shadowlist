import { memo } from 'react';
import { StyleSheet, Text, type StyleProp, type TextStyle } from 'react-native';
import { createStyles } from '../theme';

export interface RedactedTextProps {
  // Stand-in words, such as shadowhash's placeholderText. Laid out but never shown.
  children: string;
  style?: StyleProp<TextStyle>;
  numberOfLines?: number;
}

/*
 * Text drawn as one bar per word. The words are laid out in the real font, so the bars wrap
 * where text of the same word lengths would, and only the gaps between them show.
 */
export const RedactedText = memo(
  ({ children, style, numberOfLines }: RedactedTextProps) => {
    const styles = useStyles();
    const parts = children.split(/(\s+)/);
    return (
      <Text
        style={[style, styles.hidden]}
        numberOfLines={numberOfLines}
        accessibilityElementsHidden
        importantForAccessibility="no-hide-descendants"
      >
        {parts.map((part, index) =>
          index % 2 === 1 || part.length === 0 ? (
            part
          ) : (
            <Text key={index} style={styles.word}>
              {part}
            </Text>
          )
        )}
      </Text>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    hidden: {
      color: 'transparent',
    },
    word: {
      backgroundColor: theme.colors.fill,
    },
  })
);
