import { StyleSheet, View, type ColorValue } from 'react-native';
import { useTheme, type IconProps } from 'shadowlist-utils/native';

const useIconColor = (color: ColorValue | undefined): ColorValue => {
  const theme = useTheme();
  return color ?? theme.colors.label;
};

export const ViewfinderIcon = ({
  size = 22,
  color: colorProp,
  strokeWidth = 2,
}: IconProps) => {
  const color = useIconColor(colorProp);
  const ring = size * 0.6;
  const tick = size * 0.16;
  return (
    <View style={[styles.center, { width: size, height: size }]}>
      <View
        style={{
          width: ring,
          height: ring,
          borderRadius: ring / 2,
          borderWidth: strokeWidth,
          borderColor: color,
        }}
      />
      <View
        style={[
          styles.tickTop,
          { width: strokeWidth, height: tick, backgroundColor: color },
        ]}
      />
      <View
        style={[
          styles.tickBottom,
          { width: strokeWidth, height: tick, backgroundColor: color },
        ]}
      />
      <View
        style={[
          styles.tickLeft,
          { height: strokeWidth, width: tick, backgroundColor: color },
        ]}
      />
      <View
        style={[
          styles.tickRight,
          { height: strokeWidth, width: tick, backgroundColor: color },
        ]}
      />
      <View
        style={[
          styles.absolute,
          {
            width: strokeWidth * 1.6,
            height: strokeWidth * 1.6,
            borderRadius: strokeWidth,
            backgroundColor: color,
          },
        ]}
      />
    </View>
  );
};

export const EllipsisIcon = ({ size = 22, color: colorProp }: IconProps) => {
  const color = useIconColor(colorProp);
  const dot = Math.round(size * 0.18);
  return (
    <View
      style={[
        styles.dotRow,
        { width: size, height: size, paddingHorizontal: size * 0.08 },
      ]}
    >
      {[0, 1, 2].map((index) => (
        <View
          key={index}
          style={{
            width: dot,
            height: dot,
            borderRadius: dot / 2,
            backgroundColor: color,
          }}
        />
      ))}
    </View>
  );
};

const styles = StyleSheet.create({
  center: {
    alignItems: 'center',
    justifyContent: 'center',
  },
  absolute: {
    position: 'absolute',
  },
  tickTop: {
    position: 'absolute',
    top: 0,
  },
  tickBottom: {
    position: 'absolute',
    bottom: 0,
  },
  tickLeft: {
    position: 'absolute',
    left: 0,
  },
  tickRight: {
    position: 'absolute',
    right: 0,
  },
  dotRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
});
