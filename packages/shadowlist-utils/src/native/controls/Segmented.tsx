import {
  Pressable,
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface SegmentedOption<T extends string> {
  id: T;
  label: string;
}

export interface SegmentedProps<T extends string> {
  options: ReadonlyArray<SegmentedOption<T>>;
  value: T;
  onChange: (id: T) => void;
  size?: 'regular' | 'compact';
  accessibilityLabel?: string;
  style?: StyleProp<ViewStyle>;
}

/*
 * A segmented control drawn the way UIKit draws one: the track and the raised selection are
 * concentric, and a divider separates only two unselected neighbours.
 */
export const Segmented = <T extends string>({
  options,
  value,
  onChange,
  size = 'regular',
  accessibilityLabel,
  style,
}: SegmentedProps<T>) => {
  const styles = useStyles();
  const compact = size === 'compact';
  const selectedIndex = options.findIndex((option) => option.id === value);
  return (
    <View
      style={[styles.track, compact && styles.trackCompact, style]}
      accessibilityRole="tablist"
      accessibilityLabel={accessibilityLabel}
    >
      {options.map((option, index) => {
        const selected = index === selectedIndex;
        return (
          <Pressable
            key={option.id}
            accessibilityRole="tab"
            accessibilityState={{ selected }}
            accessibilityLabel={option.label}
            onPress={() => onChange(option.id)}
            style={({ pressed }) => [
              styles.segment,
              compact && styles.segmentCompact,
              selected && styles.segmentSelected,
              pressed && !selected && styles.segmentPressed,
            ]}
          >
            {index > 0 && !selected && index - 1 !== selectedIndex ? (
              <View style={styles.divider} />
            ) : null}
            <Text
              style={[
                styles.label,
                selected && styles.labelSelected,
                compact && selected && styles.labelSelectedCompact,
              ]}
              numberOfLines={1}
            >
              {option.label}
            </Text>
          </Pressable>
        );
      })}
    </View>
  );
};

const useStyles = createStyles(({ colors, fontWeight }) =>
  StyleSheet.create({
    track: {
      flexDirection: 'row',
      height: 36,
      borderRadius: 9,
      padding: 2,
      backgroundColor: colors.elevated2,
    },
    trackCompact: {
      alignSelf: 'flex-start',
      height: 34,
      borderRadius: 8,
      backgroundColor: colors.fill,
    },
    segment: {
      flex: 1,
      borderRadius: 7,
      alignItems: 'center',
      justifyContent: 'center',
    },
    segmentCompact: {
      flex: 0,
      paddingHorizontal: 10,
      borderRadius: 6,
    },
    // Opaque and raised, the way UIKit lifts the selection off the track.
    segmentSelected: {
      backgroundColor: colors.background,
      shadowColor: '#000',
      shadowOpacity: 0.1,
      shadowRadius: 2,
      shadowOffset: { width: 0, height: 1 },
    },
    segmentPressed: {
      opacity: 0.55,
    },
    divider: {
      position: 'absolute',
      left: 0,
      top: 7,
      bottom: 7,
      width: StyleSheet.hairlineWidth,
      backgroundColor: colors.separator,
    },
    label: {
      fontSize: 13,
      letterSpacing: -0.1,
      color: colors.label,
    },
    labelSelected: {
      fontWeight: fontWeight.semibold,
    },
    labelSelectedCompact: {
      color: colors.accent,
    },
  })
);
