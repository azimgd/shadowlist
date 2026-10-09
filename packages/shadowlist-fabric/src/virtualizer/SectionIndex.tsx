import { useCallback, useRef, useState } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type AccessibilityActionEvent,
  type GestureResponderEvent,
  type LayoutChangeEvent,
} from 'react-native';

interface SectionIndexProps {
  titles: ReadonlyArray<string>;
  onSelect: (index: number) => void;
}

/*
 * The section index along the trailing edge, like UITableView's and the native lists'.
 * Touching or dragging along it jumps to the title under the finger. VoiceOver and TalkBack
 * adjust it like a slider.
 */
export function SectionIndex({ titles, onSelect }: SectionIndexProps) {
  // Where the titles sit inside the index, which centers them.
  const titlesFrameRef = useRef({ y: 0, height: 0 });
  const currentRef = useRef(-1);
  const [current, setCurrent] = useState(0);

  const choose = useCallback(
    (index: number) => {
      const clamped = Math.min(titles.length - 1, Math.max(0, index));
      if (clamped === currentRef.current) return;
      currentRef.current = clamped;
      setCurrent(clamped);
      onSelect(clamped);
    },
    [titles.length, onSelect]
  );

  const handleTouch = useCallback(
    (event: GestureResponderEvent) => {
      const { y, height } = titlesFrameRef.current;
      if (height <= 0 || titles.length === 0) return;
      const position = (event.nativeEvent.locationY - y) / height;
      choose(Math.floor(position * titles.length));
    },
    [choose, titles.length]
  );

  const handleRelease = useCallback(() => {
    currentRef.current = -1;
  }, []);

  const handleTitlesLayout = useCallback((event: LayoutChangeEvent) => {
    const { y, height } = event.nativeEvent.layout;
    titlesFrameRef.current = { y, height };
  }, []);

  const handleAccessibilityAction = useCallback(
    (event: AccessibilityActionEvent) => {
      const step = event.nativeEvent.actionName === 'increment' ? 1 : -1;
      currentRef.current = -1;
      choose(current + step);
    },
    [choose, current]
  );

  return (
    <View
      style={styles.index}
      onStartShouldSetResponder={() => true}
      onMoveShouldSetResponder={() => true}
      onResponderTerminationRequest={() => false}
      onResponderGrant={handleTouch}
      onResponderMove={handleTouch}
      onResponderRelease={handleRelease}
      onResponderTerminate={handleRelease}
      accessible
      accessibilityRole="adjustable"
      accessibilityLabel="Section index"
      accessibilityValue={{ text: titles[current] ?? '' }}
      accessibilityActions={[{ name: 'increment' }, { name: 'decrement' }]}
      onAccessibilityAction={handleAccessibilityAction}
    >
      <View
        pointerEvents="none"
        style={styles.titles}
        onLayout={handleTitlesLayout}
      >
        {titles.map((title, index) => (
          <Text key={`${index}:${title}`} style={styles.title}>
            {title}
          </Text>
        ))}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  index: {
    position: 'absolute',
    top: 0,
    bottom: 0,
    right: 0,
    width: 24,
    justifyContent: 'center',
  },
  titles: {
    alignItems: 'center',
  },
  title: {
    fontSize: 11,
    fontWeight: '600',
    color: '#007AFF',
    paddingVertical: 1,
  },
});
