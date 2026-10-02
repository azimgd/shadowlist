import { memo, useMemo } from 'react';
import { StyleSheet, View, type StyleProp, type ViewStyle } from 'react-native';
import { useTheme } from '../theme';
import { clampFraction } from './ProgressBar';

export interface ProgressRingProps {
  fraction: number;
  size?: number;
  strokeWidth?: number;
  color?: string;
  trackColor?: string;
  style?: StyleProp<ViewStyle>;
}

/*
 * A ring that fills clockwise from twelve o'clock with round ends. There is no vector drawing.
 * The arc is two half rings, each turned into view behind a half-width clip, and the round
 * ends are two dots laid over them. It is decoration only and is hidden from screen readers.
 */
export const ProgressRing = memo(
  ({
    fraction,
    size = 52,
    strokeWidth = 7,
    color,
    trackColor,
    style,
  }: ProgressRingProps) => {
    const { colors } = useTheme();
    const styles = useMemo(
      () =>
        ringStyles(
          size,
          strokeWidth,
          color ?? colors.accent,
          trackColor ?? colors.accentSoft
        ),
      [size, strokeWidth, color, trackColor, colors]
    );
    const degrees = clampFraction(fraction) * 360;
    return (
      <View
        style={[styles.frame, style]}
        accessibilityElementsHidden
        importantForAccessibility="no-hide-descendants"
      >
        <View style={[styles.ring, styles.track]} />
        {degrees > 0 ? (
          <>
            {/* Twelve to six: the left half ring, turned into the right half. */}
            <View style={[styles.half, styles.rightHalf]}>
              <View
                style={[
                  styles.turn,
                  styles.fromRight,
                  { transform: [{ rotate: `${Math.min(degrees, 180)}deg` }] },
                ]}
              >
                <View style={[styles.half, styles.leftHalf]}>
                  <View style={[styles.ring, styles.arc]} />
                </View>
              </View>
            </View>
            {/* Six back to twelve: the right half ring, turned into the left half. */}
            {degrees > 180 ? (
              <View style={[styles.half, styles.leftHalf]}>
                <View
                  style={[
                    styles.turn,
                    { transform: [{ rotate: `${degrees - 180}deg` }] },
                  ]}
                >
                  <View style={[styles.half, styles.rightHalf]}>
                    <View style={[styles.ring, styles.fromRight, styles.arc]} />
                  </View>
                </View>
              </View>
            ) : null}
            <View style={[styles.cap, capAt(0, size, strokeWidth)]} />
            <View style={[styles.cap, capAt(degrees, size, strokeWidth)]} />
          </>
        ) : null}
      </View>
    );
  }
);

// Where a round end sits: on the middle of the stroke, `degrees` clockwise from the top.
function capAt(degrees: number, size: number, strokeWidth: number) {
  const radius = size / 2;
  const angle = (degrees * Math.PI) / 180;
  const middle = radius - strokeWidth / 2;
  return {
    left: radius + middle * Math.sin(angle) - strokeWidth / 2,
    top: radius - middle * Math.cos(angle) - strokeWidth / 2,
  };
}

function ringStyles(
  size: number,
  strokeWidth: number,
  color: string,
  trackColor: string
) {
  const radius = size / 2;
  return StyleSheet.create({
    frame: {
      width: size,
      height: size,
    },
    ring: {
      position: 'absolute',
      top: 0,
      left: 0,
      width: size,
      height: size,
      borderRadius: radius,
      borderWidth: strokeWidth,
    },
    track: {
      borderColor: trackColor,
    },
    arc: {
      borderColor: color,
    },
    half: {
      position: 'absolute',
      top: 0,
      width: radius,
      height: size,
      overflow: 'hidden',
    },
    leftHalf: {
      left: 0,
    },
    rightHalf: {
      left: radius,
    },
    // A full-size box inside a half. Turning it turns about the ring's center.
    turn: {
      position: 'absolute',
      top: 0,
      left: 0,
      width: size,
      height: size,
    },
    fromRight: {
      left: -radius,
    },
    cap: {
      position: 'absolute',
      width: strokeWidth,
      height: strokeWidth,
      borderRadius: strokeWidth / 2,
      backgroundColor: color,
    },
  });
}
