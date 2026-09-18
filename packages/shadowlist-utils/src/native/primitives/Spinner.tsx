import { memo } from 'react';
import {
  ActivityIndicator,
  View,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';

export interface SpinnerLabels {
  loading: string;
}

export const defaultSpinnerLabels: SpinnerLabels = {
  loading: 'Loading',
};

export interface SpinnerProps {
  size?: number;
  color?: string;
  labels?: Partial<SpinnerLabels>;
  style?: StyleProp<ViewStyle>;
}

export const Spinner = memo(
  ({ size = 20, color, labels, style }: SpinnerProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const { loading } = useLabels(defaultSpinnerLabels, labels);
    return (
      <View style={[styles.container, style]}>
        <ActivityIndicator
          size={size}
          color={color ?? theme.colors.secondaryLabel}
          accessibilityLabel={loading}
        />
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      alignItems: 'center',
      justifyContent: 'center',
      padding: theme.spacing.lg,
    },
  })
);
