import type { ComponentType } from 'react';
import { StyleSheet, View } from 'react-native';
import { useTheme, type ThemeColors } from './theme';

/*
 * Thin line icons drawn only with Views. There is no font or SVG dependency.
 * The color defaults to a theme color.
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
 * A chevron is two borders of a rotated square. It sits off center toward its tip.
 * Shift it back so it looks centered in its frame.
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
    <View style={[styles.center, { width: size, height: size }]}>
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
        style={[
          styles.folderTab,
          { width: size * 0.44, height: size * 0.28, backgroundColor: color },
        ]}
      />
      <View
        style={[
          styles.folderBody,
          { top: size * 0.16, backgroundColor: color },
        ]}
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
    <View style={[styles.center, { width: size, height: size }]}>
      <View
        style={[
          styles.docPage,
          {
            width: w,
            height: h,
            borderWidth: strokeWidth,
            borderColor: color,
            paddingHorizontal: w * 0.18,
            gap: h * 0.16,
          },
        ]}
      >
        <View
          style={{
            height: strokeWidth,
            backgroundColor: color,
            borderRadius: strokeWidth,
          }}
        />
        <View
          style={[
            styles.docShortLine,
            {
              height: strokeWidth,
              backgroundColor: color,
              borderRadius: strokeWidth,
            },
          ]}
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
      style={[
        styles.justifyCenter,
        { width: size, height: size, gap: size * 0.22 },
      ]}
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
    <View style={[styles.center, { width: size, height: size }]}>
      <View
        style={{
          width: strokeWidth,
          height: size * 0.56,
          backgroundColor: color,
          borderRadius: strokeWidth,
          marginTop: size * 0.08,
        }}
      />
      <View style={[styles.arrowHeadFrame, { top: size * 0.16 }]}>
        <View
          style={[
            styles.rotateUp,
            {
              width: head,
              height: head,
              borderTopWidth: strokeWidth,
              borderRightWidth: strokeWidth,
              borderColor: color,
            },
          ]}
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
    <View style={[styles.center, { width: size, height: size }]}>
      <View
        style={{
          width: bar,
          height: strokeWidth,
          backgroundColor: color,
          borderRadius: strokeWidth,
        }}
      />
      <View
        style={[
          styles.absolute,
          {
            top: (size - bar) / 2,
            left: (size - strokeWidth) / 2,
            width: strokeWidth,
            height: bar,
            backgroundColor: color,
            borderRadius: strokeWidth,
          },
        ]}
      />
    </View>
  );
};

/*
 * A microphone. An outlined capsule in a U shaped holder on a short stem and base.
 */
export const MicIcon = ({
  size = 20,
  color: colorProp,
  strokeWidth = 1.6,
}: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const capsuleWidth = size * 0.34;
  const capsuleHeight = size * 0.5;
  const holderWidth = size * 0.58;
  const holderTop = size * 0.32;
  const holderHeight = size * 0.36;
  const baseWidth = size * 0.32;
  return (
    <View style={{ width: size, height: size }}>
      <View
        style={[
          styles.absolute,
          {
            top: size * 0.06,
            left: (size - capsuleWidth) / 2,
            width: capsuleWidth,
            height: capsuleHeight,
            borderRadius: capsuleWidth / 2,
            borderWidth: strokeWidth,
            borderColor: color,
          },
        ]}
      />
      <View
        style={[
          styles.absolute,
          styles.noTopBorder,
          {
            top: holderTop,
            left: (size - holderWidth) / 2,
            width: holderWidth,
            height: holderHeight,
            borderWidth: strokeWidth,
            borderColor: color,
            borderBottomLeftRadius: holderWidth / 2,
            borderBottomRightRadius: holderWidth / 2,
          },
        ]}
      />
      <View
        style={[
          styles.absolute,
          {
            top: holderTop + holderHeight,
            left: (size - strokeWidth) / 2,
            width: strokeWidth,
            height: size * 0.14,
            backgroundColor: color,
          },
        ]}
      />
      <View
        style={[
          styles.absolute,
          {
            top: holderTop + holderHeight + size * 0.14 - strokeWidth / 2,
            left: (size - baseWidth) / 2,
            width: baseWidth,
            height: strokeWidth,
            borderRadius: strokeWidth,
            backgroundColor: color,
          },
        ]}
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
    <View style={[styles.rotate45, { width: size, height: size }]}>
      <PlusIcon size={size} color={color} strokeWidth={strokeWidth} />
    </View>
  );
};

export const StopIcon = ({ size = 20, color: colorProp }: IconProps) => {
  const color = useIconColor(colorProp, 'label');
  const side = size * 0.4;
  return (
    <View style={[styles.center, { width: size, height: size }]}>
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
    <View style={[styles.center, { width: size, height: size }]}>
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
    width: sheet,
    height: sheet,
    borderRadius: sheet * 0.22,
    borderWidth: strokeWidth,
    borderColor: color,
  };
  return (
    <View style={{ width: size, height: size }}>
      <View
        style={[
          styles.absolute,
          sheetStyle,
          { left: size * 0.16, top: size * 0.16 },
        ]}
      />
      <View
        style={[
          styles.absolute,
          sheetStyle,
          { left: size * 0.34, top: size * 0.34 },
        ]}
      />
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
    <View style={[styles.center, { width: size, height: size }]}>
      <View
        style={[
          styles.retryRing,
          {
            width: ring,
            height: ring,
            borderRadius: ring / 2,
            borderWidth: strokeWidth,
            borderColor: color,
          },
        ]}
      />
      <View
        style={[
          styles.absolute,
          styles.rotateRetryHead,
          {
            top: size * 0.2,
            left: size * 0.62,
            width: head,
            height: head,
            borderTopWidth: strokeWidth,
            borderRightWidth: strokeWidth,
            borderColor: color,
          },
        ]}
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
        style={[
          styles.absolute,
          styles.noTopBorder,
          styles.trayCorners,
          {
            left: (size - trayWidth) / 2,
            bottom: size * 0.12,
            width: trayWidth,
            height: size * 0.4,
            borderWidth: strokeWidth,
            borderColor: color,
          },
        ]}
      />
      <View
        style={[
          styles.absolute,
          {
            left: (size - strokeWidth) / 2,
            top: size * 0.1,
            width: strokeWidth,
            height: size * 0.52,
            backgroundColor: color,
            borderRadius: strokeWidth,
          },
        ]}
      />
      <View
        style={[
          styles.absolute,
          styles.rotateUp,
          {
            left: (size - head) / 2,
            top: size * 0.12,
            width: head,
            height: head,
            borderTopWidth: strokeWidth,
            borderRightWidth: strokeWidth,
            borderColor: color,
          },
        ]}
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
        style={[
          styles.absolute,
          {
            left: offset,
            top: offset,
            width: side,
            height: side,
            borderRadius: side * 0.14,
            backgroundColor: color,
          },
        ]}
      />
      <View
        style={[
          styles.absolute,
          styles.rotate45,
          {
            left: offset,
            top: offset,
            width: side,
            height: side,
            borderRadius: side * 0.14,
            backgroundColor: color,
          },
        ]}
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
    <View style={[styles.center, { width: size, height: size }]}>
      <View style={styles.pencilBody}>
        <View
          style={[
            styles.pencilBarrel,
            {
              width: barrel,
              height: size * 0.5,
              borderWidth: strokeWidth,
              borderColor: color,
            },
          ]}
        />
        <View
          style={[
            styles.pencilTip,
            {
              borderLeftWidth: barrel / 2,
              borderRightWidth: barrel / 2,
              borderTopWidth: size * 0.16,
              borderTopColor: color,
            },
          ]}
        />
      </View>
    </View>
  );
};

export const SearchIcon = ({
  size = 17,
  color: colorProp,
  strokeWidth = 1.8,
}: IconProps) => {
  const color = useIconColor(colorProp, 'secondaryLabel');
  const lens = size * 0.62;
  const handle = size * 0.34;
  return (
    <View style={{ width: size, height: size }}>
      <View
        style={[
          styles.absolute,
          {
            top: size * 0.06,
            left: size * 0.06,
            width: lens,
            height: lens,
            borderRadius: lens / 2,
            borderWidth: strokeWidth,
            borderColor: color,
          },
        ]}
      />
      <View
        style={[
          styles.absolute,
          styles.rotate45,
          styles.originLeft,
          {
            top: size * 0.06 + lens * 0.85 + handle * 0.15,
            left: size * 0.06 + lens * 0.85 - strokeWidth / 2,
            width: handle,
            height: strokeWidth,
            borderRadius: strokeWidth,
            backgroundColor: color,
          },
        ]}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  center: {
    alignItems: 'center',
    justifyContent: 'center',
  },
  justifyCenter: {
    justifyContent: 'center',
  },
  absolute: {
    position: 'absolute',
  },
  rotate45: {
    transform: [{ rotate: '45deg' }],
  },
  rotateUp: {
    transform: [{ rotate: '-45deg' }],
  },
  rotateRetryHead: {
    transform: [{ rotate: '100deg' }],
  },
  originLeft: {
    transformOrigin: 'left center',
  },
  noTopBorder: {
    borderTopWidth: 0,
  },
  trayCorners: {
    borderBottomLeftRadius: 3,
    borderBottomRightRadius: 3,
  },
  folderTab: {
    position: 'absolute',
    top: 0,
    left: 0,
    borderTopLeftRadius: 2.5,
    borderTopRightRadius: 2.5,
  },
  folderBody: {
    position: 'absolute',
    left: 0,
    right: 0,
    bottom: 0,
    borderRadius: 3,
  },
  docPage: {
    borderRadius: 2.5,
    justifyContent: 'center',
  },
  docShortLine: {
    width: '70%',
  },
  arrowHeadFrame: {
    position: 'absolute',
    left: 0,
    right: 0,
    alignItems: 'center',
  },
  retryRing: {
    borderTopColor: 'transparent',
    transform: [{ rotate: '45deg' }],
  },
  pencilBody: {
    alignItems: 'center',
    transform: [{ rotate: '45deg' }],
  },
  pencilBarrel: {
    borderTopLeftRadius: 2,
    borderTopRightRadius: 2,
  },
  pencilTip: {
    width: 0,
    height: 0,
    borderLeftColor: 'transparent',
    borderRightColor: 'transparent',
  },
});
