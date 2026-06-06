/* eslint-disable react-native/no-inline-styles -- icon geometry is derived from
 * the size/color/weight props, so the shape styles are intentionally dynamic. */
import { View } from 'react-native';
import { colors } from './theme';

/*
 * Lightweight SF-Symbol-style icons drawn purely from <View> primitives — no
 * emoji, no font dependency, no SVG. Every glyph is crisp at any size and tints
 * via the `color` prop so it inherits the app accent.
 */

type IconProps = {
  size?: number;
  color?: string;
  weight?: number;
};

type Direction = 'up' | 'down' | 'left' | 'right';

const CHEVRON_ROTATION: Record<Direction, string> = {
  right: '45deg',
  down: '135deg',
  left: '225deg',
  up: '-45deg',
};

// A chevron is two borders of a rotated square, so its ink sits ~0.35·side off
// the box center toward the apex. Counter-translate along the pointing axis so
// the glyph is optically centered in its frame.
const CHEVRON_SHIFT: Record<Direction, { x: number; y: number }> = {
  up: { x: 0, y: 1 },
  down: { x: 0, y: -1 },
  left: { x: 1, y: 0 },
  right: { x: -1, y: 0 },
};

export const Chevron = ({
  size = 17,
  color = colors.label,
  weight = 2,
  direction = 'right',
}: IconProps & { direction?: Direction }) => {
  const side = size * 0.42;
  const k = side * 0.3535;
  const shift = CHEVRON_SHIFT[direction];
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View
        style={{
          transform: [{ translateX: k * shift.x }, { translateY: k * shift.y }],
        }}
      >
        <View
          style={{
            width: side,
            height: side,
            borderTopWidth: weight,
            borderRightWidth: weight,
            borderColor: color,
            transform: [{ rotate: CHEVRON_ROTATION[direction] }],
          }}
        />
      </View>
    </View>
  );
};

// Viewfinder / locate target — replaces the 🎯 "scroll to random" affordance.
export const Viewfinder = ({
  size = 22,
  color = colors.label,
  weight = 2,
}: IconProps) => {
  const ring = size * 0.6;
  const tick = size * 0.16;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View
        style={{
          width: ring,
          height: ring,
          borderRadius: ring / 2,
          borderWidth: weight,
          borderColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          top: 0,
          width: weight,
          height: tick,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          bottom: 0,
          width: weight,
          height: tick,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          left: 0,
          height: weight,
          width: tick,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          right: 0,
          height: weight,
          width: tick,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          width: weight * 1.6,
          height: weight * 1.6,
          borderRadius: weight,
          backgroundColor: color,
        }}
      />
    </View>
  );
};

// Filled folder, tinted with the accent (Files-app style).
export const Folder = ({ size = 18, color = colors.accent }: IconProps) => {
  return (
    <View style={{ width: size, height: size * 0.82 }}>
      <View
        style={{
          position: 'absolute',
          top: 0,
          left: 0,
          width: size * 0.44,
          height: size * 0.28,
          backgroundColor: color,
          borderTopLeftRadius: 2.5,
          borderTopRightRadius: 2.5,
        }}
      />
      <View
        style={{
          position: 'absolute',
          left: 0,
          right: 0,
          bottom: 0,
          top: size * 0.16,
          backgroundColor: color,
          borderRadius: 3,
        }}
      />
    </View>
  );
};

// Outlined document with two text lines.
export const Doc = ({
  size = 18,
  color = colors.secondaryLabel,
  weight = 1.6,
}: IconProps) => {
  const w = size * 0.72;
  const h = size * 0.9;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View
        style={{
          width: w,
          height: h,
          borderWidth: weight,
          borderColor: color,
          borderRadius: 2.5,
          paddingHorizontal: w * 0.18,
          justifyContent: 'center',
          gap: h * 0.16,
        }}
      >
        <View
          style={{
            height: weight,
            backgroundColor: color,
            borderRadius: weight,
          }}
        />
        <View
          style={{
            height: weight,
            width: '70%',
            backgroundColor: color,
            borderRadius: weight,
          }}
        />
      </View>
    </View>
  );
};

// Three-line reorder grip.
export const Grip = ({
  size = 20,
  color = colors.tertiaryLabel,
  weight = 1.75,
}: IconProps) => {
  const line = {
    width: size,
    height: weight,
    backgroundColor: color,
    borderRadius: weight,
  };
  return (
    <View
      style={{
        width: size,
        height: size,
        justifyContent: 'center',
        gap: size * 0.22,
      }}
    >
      <View style={line} />
      <View style={line} />
      <View style={line} />
    </View>
  );
};

export const HalfCircle = ({
  size = 22,
  color = colors.label,
  weight = 2,
}: IconProps) => {
  const d = size * 0.62;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View
        style={{
          width: d,
          height: d,
          borderRadius: d / 2,
          borderWidth: weight,
          borderColor: color,
          overflow: 'hidden',
        }}
      >
        <View
          style={{
            position: 'absolute',
            right: 0,
            top: 0,
            bottom: 0,
            width: d / 2,
            backgroundColor: color,
          }}
        />
      </View>
    </View>
  );
};

export const CircleSlash = ({
  size = 22,
  color = colors.label,
  weight = 2,
}: IconProps) => {
  const d = size * 0.62;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View
        style={{
          width: d,
          height: d,
          borderRadius: d / 2,
          borderWidth: weight,
          borderColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          width: weight,
          height: d,
          backgroundColor: color,
          borderRadius: weight,
          transform: [{ rotate: '45deg' }],
        }}
      />
    </View>
  );
};

export const Bell = ({
  size = 22,
  color = colors.label,
  weight = 2,
}: IconProps) => {
  const bodyW = size * 0.44;
  const bodyH = size * 0.42;
  const baseW = size * 0.6;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View style={{ alignItems: 'center' }}>
        <View
          style={{
            width: bodyW,
            height: bodyH,
            borderTopLeftRadius: bodyW / 2,
            borderTopRightRadius: bodyW / 2,
            borderColor: color,
            borderWidth: weight,
            borderBottomWidth: 0,
          }}
        />
        <View
          style={{
            width: baseW,
            height: weight,
            backgroundColor: color,
            borderRadius: weight,
          }}
        />
        <View
          style={{
            width: weight * 1.8,
            height: weight * 1.8,
            borderRadius: weight,
            backgroundColor: color,
            marginTop: weight * 0.7,
          }}
        />
      </View>
    </View>
  );
};

export const Globe = ({
  size = 22,
  color = colors.label,
  weight = 1.8,
}: IconProps) => {
  const d = size * 0.64;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View
        style={{
          width: d,
          height: d,
          borderRadius: d / 2,
          borderWidth: weight,
          borderColor: color,
          alignItems: 'center',
          justifyContent: 'center',
        }}
      >
        <View
          style={{
            position: 'absolute',
            width: d * 0.46,
            height: d,
            borderRadius: d * 0.23,
            borderWidth: weight,
            borderColor: color,
          }}
        />
        <View
          style={{
            position: 'absolute',
            width: d,
            height: weight,
            backgroundColor: color,
          }}
        />
      </View>
    </View>
  );
};

export const Swatch = ({ size = 22, color = colors.label }: IconProps) => {
  const d = size * 0.56;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View
        style={{
          width: d,
          height: d,
          borderRadius: d * 0.3,
          backgroundColor: color,
        }}
      />
    </View>
  );
};

export const ArrowUp = ({
  size = 20,
  color = colors.label,
  weight = 2.2,
}: IconProps) => {
  const head = size * 0.36;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View
        style={{
          width: weight,
          height: size * 0.56,
          backgroundColor: color,
          borderRadius: weight,
          marginTop: size * 0.08,
        }}
      />
      <View
        style={{
          position: 'absolute',
          top: size * 0.16,
          left: 0,
          right: 0,
          alignItems: 'center',
        }}
      >
        <View
          style={{
            width: head,
            height: head,
            borderTopWidth: weight,
            borderRightWidth: weight,
            borderColor: color,
            transform: [{ rotate: '-45deg' }],
          }}
        />
      </View>
    </View>
  );
};
