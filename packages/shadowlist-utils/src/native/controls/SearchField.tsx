import { memo, type ReactNode } from 'react';
import {
  StyleSheet,
  TextInput,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { SearchIcon } from '../icons';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';

export interface SearchFieldLabels {
  placeholder: string;
}

export const defaultSearchFieldLabels: SearchFieldLabels = {
  placeholder: 'Search',
};

export interface SearchFieldProps {
  value: string;
  onChange: (text: string) => void;
  icon?: ReactNode;
  labels?: Partial<SearchFieldLabels>;
  style?: StyleProp<ViewStyle>;
}

export const SearchField = memo(
  ({ value, onChange, icon, labels, style }: SearchFieldProps) => {
    const styles = useStyles();
    const { colors } = useTheme();
    const { placeholder } = useLabels(defaultSearchFieldLabels, labels);
    return (
      <View style={[styles.field, style]}>
        {icon ?? <SearchIcon size={16} />}
        <TextInput
          value={value}
          onChangeText={onChange}
          placeholder={placeholder}
          placeholderTextColor={colors.secondaryLabel}
          style={styles.input}
          autoCorrect={false}
          autoCapitalize="none"
          clearButtonMode="while-editing"
          accessibilityLabel={placeholder}
        />
      </View>
    );
  }
);

const useStyles = createStyles(({ colors, typography }) =>
  StyleSheet.create({
    field: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: 6,
      height: 36,
      paddingHorizontal: 10,
      borderRadius: 10,
      backgroundColor: colors.elevated2,
    },
    input: {
      flex: 1,
      padding: 0,
      fontSize: typography.body.fontSize,
      letterSpacing: typography.body.letterSpacing,
      color: colors.label,
    },
  })
);
