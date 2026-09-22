import { memo } from 'react';
import {
  Image,
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles, useTheme } from '../theme';
import { getAvatarColor, getInitials } from './avatarAppearance';

export interface AvatarProps {
  name: string;
  uri?: string;
  // Defaults to a color from theme.colors.avatarPalette picked by name.
  color?: string;
  size?: number;
  style?: StyleProp<ViewStyle>;
}

export const Avatar = memo(
  ({ name, uri, color, size = 40, style }: AvatarProps) => {
    const theme = useTheme();
    const styles = useStyles();
    return (
      <View
        style={[
          styles.avatar,
          {
            width: size,
            height: size,
            borderRadius: size / 2,
            backgroundColor:
              color ?? getAvatarColor(name, theme.colors.avatarPalette),
          },
          style,
        ]}
        accessibilityElementsHidden
        importantForAccessibility="no-hide-descendants"
      >
        {/* Initials stay underneath, so they show while the image loads or when it fails. */}
        <Text
          style={[styles.initials, { fontSize: Math.floor(size * 0.43) }]}
          allowFontScaling={false}
          numberOfLines={1}
        >
          {getInitials(name)}
        </Text>
        {uri ? (
          <Image source={{ uri }} style={StyleSheet.absoluteFill} />
        ) : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    avatar: {
      alignItems: 'center',
      justifyContent: 'center',
      overflow: 'hidden',
    },
    initials: {
      color: theme.colors.label,
      fontWeight: theme.fontWeight.semibold,
    },
  })
);
