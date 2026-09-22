/* eslint-disable react-native/no-inline-styles -- icon geometry is derived from
 * the size/color/strokeWidth props, so the shape styles are intentionally dynamic. */
import { View } from 'react-native';
import { useTheme, type IconProps } from 'shadowlist-utils/native';

// Header action glyph for the demo, drawn like the package icons.

const useIconColor = (color: string | undefined): string => {
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
        }}
      />
      <View
        style={{
          position: 'absolute',
          top: 0,
          width: strokeWidth,
          height: tick,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          bottom: 0,
          width: strokeWidth,
          height: tick,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          left: 0,
          height: strokeWidth,
          width: tick,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          right: 0,
          height: strokeWidth,
          width: tick,
          backgroundColor: color,
        }}
      />
      <View
        style={{
          position: 'absolute',
          width: strokeWidth * 1.6,
          height: strokeWidth * 1.6,
          borderRadius: strokeWidth,
          backgroundColor: color,
        }}
      />
    </View>
  );
};
