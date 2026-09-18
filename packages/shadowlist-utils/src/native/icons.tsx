/* eslint-disable react-native/no-inline-styles -- icon geometry is derived from
 * the size/color/strokeWidth props, so the shape styles are intentionally dynamic. */
import type { ComponentType } from 'react';
import { View } from 'react-native';
import { useTheme, type ThemeColors } from './theme';

/*
 * Light-stroke, SF-Symbol-style icons drawn purely from <View> primitives: no
 * emoji, no font dependency, no SVG. `color` defaults to a theme color.
 */

export interface IconProps {
  size?: number;
  color?: string;
  strokeWidth?: number;
}

export type IconComponent = ComponentType<IconProps>;

type IconColor = Exclude<keyof ThemeColors, 'avatarPalette'>;

const useIconColor = (
  color: string | undefined,
  fallback: IconColor
): string => {
  const theme = useTheme();
  return color ?? theme.colors[fallback];
};

type Direction = 'up' | 'down' | 'left' | 'right';

const CHEVRON_ROTATION: Record<Direction, string> = {
  right: '45deg',
  down: '135deg',
  left: '225deg',
  up: '-45deg',
};

/*
 * A chevron is two borders of a rotated square, so its ink sits ~0.35·side off
 * the box center toward the apex. Counter-translate along the pointing axis so
 * the glyph is optically centered in its frame.
 */
const CHEVRON_SHIFT: Record<Direction, { x: number; y: number }> = {
  up: { x: 0, y: 1 },
  down: { x: 0, y: -1 },
  left: { x: 1, y: 0 },
  right: { x: -1, y: 0 },
};

export const ChevronIcon = ({
  size = 17,
  color: colorProp,
  strokeWidth = 2,
  direction = 'right',
}: IconProps & { direction?: Direction }) => {
  const color = useIconColor(colorProp, 'label');
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
            borderTopWidth: strokeWidth,
            borderRightWidth: strokeWidth,
            borderColor: color,
            transform: [{ rotate: CHEVRON_ROTATION[direction] }],
          }}
        />
      </View>
    </View>
  );
};

export const FolderIcon = ({ size = 18, color: colorProp }: IconProps) => {
  const color = useIconColor(colorProp, 'accent');
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

export const DocIcon = ({
  size = 18,
  color: colorProp,
  strokeWidth = 1.6,
}: IconProps) => {
  const color = useIconColor(colorProp, 'secondaryLabel');
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
          borderWidth: strokeWidth,
          borderColor: color,
          borderRadius: 2.5,
          paddingHorizontal: w * 0.18,
          justifyContent: 'center',
          gap: h * 0.16,
        }}
      >
        <View
          style={{
            height: strokeWidth,
            backgroundColor: color,
            borderRadius: strokeWidth,
          }}
        />
        <View
          style={{
            height: strokeWidth,
            width: '70%',
            backgroundColor: color,
            borderRadius: strokeWidth,
          }}
        />
      </View>
    </View>
  );
};

export const GripIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 1.75,
}: IconProps) => {
  const color = useIconColor(colorProp, 'tertiaryLabel');
  const line = {
    width: size,
    height: strokeWidth,
    backgroundColor: color,
    borderRadius: strokeWidth,
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

export const ArrowUpIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 2.2,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
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
          width: strokeWidth,
          height: size * 0.56,
          backgroundColor: color,
          borderRadius: strokeWidth,
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
            borderTopWidth: strokeWidth,
            borderRightWidth: strokeWidth,
            borderColor: color,
            transform: [{ rotate: '-45deg' }],
          }}
        />
      </View>
    </View>
  );
};

export const PlusIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 2,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const bar = size * 0.56;
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
          width: bar,
          height: strokeWidth,
          backgroundColor: color,
          borderRadius: strokeWidth,
        }}
      />
      <View
        style={{
          position: 'absolute',
          top: (size - bar) / 2,
          left: (size - strokeWidth) / 2,
          width: strokeWidth,
          height: bar,
          backgroundColor: color,
          borderRadius: strokeWidth,
        }}
      />
    </View>
  );
};

export const CloseIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 2,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  return (
    <View
      style={{
        width: size,
        height: size,
        transform: [{ rotate: '45deg' }],
      }}
    >
      <PlusIcon size={size} color={color} strokeWidth={strokeWidth} />
    </View>
  );
};

export const StopIcon = ({ size = 20, color: colorProp }: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const side = size * 0.4;
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
          width: side,
          height: side,
          borderRadius: side * 0.2,
          backgroundColor: color,
        }}
      />
    </View>
  );
};

export const CheckIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 2.2,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
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
          width: size * 0.26,
          height: size * 0.5,
          borderRightWidth: strokeWidth,
          borderBottomWidth: strokeWidth,
          borderColor: color,
          transform: [{ translateY: -size * 0.06 }, { rotate: '45deg' }],
        }}
      />
    </View>
  );
};

export const CopyIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 1.6,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const sheet = size * 0.5;
  const sheetStyle = {
    position: 'absolute' as const,
    width: sheet,
    height: sheet,
    borderRadius: sheet * 0.22,
    borderWidth: strokeWidth,
    borderColor: color,
  };
  return (
    <View style={{ width: size, height: size }}>
      <View style={[sheetStyle, { left: size * 0.16, top: size * 0.16 }]} />
      <View style={[sheetStyle, { left: size * 0.34, top: size * 0.34 }]} />
    </View>
  );
};

export const RetryIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 1.8,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const ring = size * 0.6;
  const head = size * 0.2;
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
          borderWidth: strokeWidth,
          borderColor: color,
          borderTopColor: 'transparent',
          transform: [{ rotate: '45deg' }],
        }}
      />
      <View
        style={{
          position: 'absolute',
          top: size * 0.2,
          left: size * 0.62,
          width: head,
          height: head,
          borderTopWidth: strokeWidth,
          borderRightWidth: strokeWidth,
          borderColor: color,
          transform: [{ rotate: '100deg' }],
        }}
      />
    </View>
  );
};

export const ShareIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 1.8,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const trayWidth = size * 0.56;
  const head = size * 0.22;
  return (
    <View style={{ width: size, height: size }}>
      <View
        style={{
          position: 'absolute',
          left: (size - trayWidth) / 2,
          bottom: size * 0.12,
          width: trayWidth,
          height: size * 0.4,
          borderWidth: strokeWidth,
          borderTopWidth: 0,
          borderColor: color,
          borderBottomLeftRadius: 3,
          borderBottomRightRadius: 3,
        }}
      />
      <View
        style={{
          position: 'absolute',
          left: (size - strokeWidth) / 2,
          top: size * 0.1,
          width: strokeWidth,
          height: size * 0.52,
          backgroundColor: color,
          borderRadius: strokeWidth,
        }}
      />
      <View
        style={{
          position: 'absolute',
          left: (size - head) / 2,
          top: size * 0.12,
          width: head,
          height: head,
          borderTopWidth: strokeWidth,
          borderRightWidth: strokeWidth,
          borderColor: color,
          transform: [{ rotate: '-45deg' }],
        }}
      />
    </View>
  );
};

export const SparkleIcon = ({ size = 20, color: colorProp }: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const side = size * 0.46;
  const offset = (size - side) / 2;
  return (
    <View style={{ width: size, height: size }}>
      <View
        style={{
          position: 'absolute',
          left: offset,
          top: offset,
          width: side,
          height: side,
          borderRadius: side * 0.14,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          left: offset,
          top: offset,
          width: side,
          height: side,
          borderRadius: side * 0.14,
          backgroundColor: color,
          transform: [{ rotate: '45deg' }],
        }}
      />
    </View>
  );
};

export const PencilIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 1.6,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const barrel = size * 0.22;
  return (
    <View
      style={{
        width: size,
        height: size,
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      <View style={{ alignItems: 'center', transform: [{ rotate: '45deg' }] }}>
        <View
          style={{
            width: barrel,
            height: size * 0.5,
            borderWidth: strokeWidth,
            borderColor: color,
            borderTopLeftRadius: 2,
            borderTopRightRadius: 2,
          }}
        />
        <View
          style={{
            width: 0,
            height: 0,
            borderLeftWidth: barrel / 2,
            borderRightWidth: barrel / 2,
            borderTopWidth: size * 0.16,
            borderLeftColor: 'transparent',
            borderRightColor: 'transparent',
            borderTopColor: color,
          }}
        />
      </View>
    </View>
  );
};
