import { memo, useRef, useState, type ComponentRef } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type AccessibilityActionEvent,
  type GestureResponderEvent,
} from 'react-native';
import { createStyles } from 'shadowlist-utils/native';
import { haptics } from './haptics';

interface SectionIndexProps {
  titles: string[];
  onSelect: (index: number) => void;
}

export const SectionIndex = memo(({ titles, onSelect }: SectionIndexProps) => {
  const styles = useStyles();
  const viewRef = useRef<ComponentRef<typeof View>>(null);
  /*
   * Window frame. locationY is relative to the touched letter, so touches use pageY instead.
   */
  const frameRef = useRef({ top: 0, height: 0 });
  const lastRef = useRef(-1);
  const [current, setCurrent] = useState(0);

  const select = (index: number) => {
    const clamped = Math.max(0, Math.min(titles.length - 1, index));
    if (clamped === lastRef.current) return;
    lastRef.current = clamped;
    setCurrent(clamped);
    haptics.selection();
    onSelect(clamped);
  };

  const selectAt = (event: GestureResponderEvent) => {
    const { top, height } = frameRef.current;
    if (height <= 0) return;
    const ratio = (event.nativeEvent.pageY - top) / height;
    select(Math.floor(ratio * titles.length));
  };

  const handleAccessibilityAction = (event: AccessibilityActionEvent) => {
    const step = event.nativeEvent.actionName === 'increment' ? 1 : -1;
    select(current + step);
  };

  return (
    <View style={styles.rail} pointerEvents="box-none">
      <View
        ref={viewRef}
        style={styles.column}
        onLayout={() => {
          viewRef.current?.measureInWindow((_x, top, _width, height) => {
            frameRef.current = { top, height };
          });
        }}
        onStartShouldSetResponder={() => true}
        onMoveShouldSetResponder={() => true}
        onResponderGrant={selectAt}
        onResponderMove={selectAt}
        onResponderRelease={() => {
          lastRef.current = -1;
        }}
        onResponderTerminationRequest={() => false}
        accessible
        accessibilityRole="adjustable"
        accessibilityLabel="Section index"
        accessibilityValue={{ text: titles[current] }}
        accessibilityActions={[{ name: 'increment' }, { name: 'decrement' }]}
        onAccessibilityAction={handleAccessibilityAction}
      >
        {titles.map((title) => (
          <Text key={title} style={styles.letter} maxFontSizeMultiplier={1.2}>
            {title}
          </Text>
        ))}
      </View>
    </View>
  );
});

const useStyles = createStyles(({ colors, fontWeight }) =>
  StyleSheet.create({
    rail: {
      position: 'absolute',
      right: 0,
      top: 0,
      bottom: 0,
      justifyContent: 'center',
    },
    column: {
      width: 24,
      paddingVertical: 6,
      alignItems: 'center',
    },
    letter: {
      color: colors.accent,
      fontSize: 11,
      lineHeight: 14,
      fontWeight: fontWeight.semibold,
    },
  })
);
