import { memo, useState } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type TextLayoutEvent,
  type TextStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface RedactedTextProps {
  // Stand-in words, such as shadowhash's placeholderText. Laid out but never shown.
  children: string;
  style?: StyleProp<TextStyle>;
  numberOfLines?: number;
}

// Share of a line's height the bar covers, centred, so the lines stay apart.
const BAR_HEIGHT = 0.62;

/*
 * Text drawn as one bar per line. The words are laid out in the real font and hidden, so the
 * bars wrap where text of the same word lengths would, and a blank line stays blank.
 */
export const RedactedText = memo(
  ({ children, style, numberOfLines }: RedactedTextProps) => {
    const styles = useStyles();
    const [lines, setLines] = useState<TextLayoutEvent['nativeEvent']['lines']>(
      []
    );
    return (
      <View
        accessibilityElementsHidden
        importantForAccessibility="no-hide-descendants"
      >
        <Text
          style={[style, styles.hidden]}
          numberOfLines={numberOfLines}
          onTextLayout={(event) => setLines(event.nativeEvent.lines)}
        >
          {children}
        </Text>
        {lines.map((line, index) =>
          line.text.trim().length === 0 ? null : (
            <View
              key={index}
              style={[
                styles.bar,
                {
                  left: line.x,
                  top: line.y + (line.height * (1 - BAR_HEIGHT)) / 2,
                  width: line.width,
                  height: line.height * BAR_HEIGHT,
                  borderRadius: (line.height * BAR_HEIGHT) / 2,
                },
              ]}
            />
          )
        )}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    hidden: {
      color: 'transparent',
    },
    bar: {
      position: 'absolute',
      backgroundColor: theme.colors.fill,
    },
  })
);
