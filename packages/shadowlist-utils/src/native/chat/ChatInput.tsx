import { useState } from 'react';
import {
  View,
  TextInput,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { ArrowUpIcon } from '../icons';
import { keyboardAppearanceFor } from '../internal/keyboardAppearance';
import { defaultChatLabels, type ChatLabels } from './labels';

export interface ChatInputProps {
  // Receives the trimmed text. Uncontrolled inputs clear themselves; controlled ones clear via `value`.
  onSend: (text: string) => void;
  value?: string;
  defaultValue?: string;
  onChangeText?: (text: string) => void;
  disabled?: boolean;
  maxLength?: number;
  labels?: Partial<ChatLabels>;
  style?: StyleProp<ViewStyle>;
}

export const ChatInput = ({
  onSend,
  value,
  defaultValue = '',
  onChangeText,
  disabled = false,
  maxLength = 500,
  labels,
  style,
}: ChatInputProps) => {
  const theme = useTheme();
  const styles = useStyles();
  const l = useLabels(defaultChatLabels, labels);
  const insets = useSafeAreaInsets();
  const [uncontrolledText, setUncontrolledText] = useState(defaultValue);
  const isControlled = value !== undefined;
  const text = isControlled ? value : uncontrolledText;
  const canSend = !disabled && text.trim().length > 0;

  const handleChangeText = (next: string) => {
    if (!isControlled) {
      setUncontrolledText(next);
    }
    onChangeText?.(next);
  };

  const handleSend = () => {
    if (!canSend) {
      return;
    }
    onSend(text.trim());
    if (!isControlled) {
      setUncontrolledText('');
    }
  };

  return (
    <View
      style={[
        styles.container,
        { paddingBottom: insets.bottom || theme.spacing.sm },
        style,
      ]}
    >
      <View style={styles.inputContainer}>
        <TextInput
          style={styles.input}
          value={text}
          onChangeText={handleChangeText}
          placeholder={l.placeholder}
          placeholderTextColor={theme.colors.secondaryLabel}
          keyboardAppearance={keyboardAppearanceFor(theme)}
          editable={!disabled}
          multiline
          maxLength={maxLength}
        />
      </View>
      <Pressable
        style={[styles.sendButton, !canSend && styles.sendButtonDisabled]}
        onPress={handleSend}
        disabled={!canSend}
        accessibilityRole="button"
        accessibilityLabel={l.send}
        accessibilityState={{ disabled: !canSend }}
      >
        <ArrowUpIcon
          size={20}
          color={canSend ? theme.colors.onAccent : theme.colors.secondaryLabel}
          strokeWidth={2.4}
        />
      </Pressable>
    </View>
  );
};

const useStyles = createStyles(({ colors, typography, radius, spacing }) =>
  StyleSheet.create({
    container: {
      flexDirection: 'row',
      alignItems: 'flex-end',
      paddingHorizontal: spacing.sm,
      paddingTop: spacing.sm,
      backgroundColor: colors.background,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
    },
    inputContainer: {
      flex: 1,
      backgroundColor: colors.background,
      borderRadius: radius.lg + 2,
      borderWidth: StyleSheet.hairlineWidth,
      borderColor: colors.separator,
      paddingHorizontal: spacing.md,
      paddingVertical: 7,
      marginRight: spacing.sm,
      minHeight: 36,
      maxHeight: 120,
    },
    input: {
      color: colors.label,
      ...typography.body,
      padding: 0,
      margin: 0,
    },
    sendButton: {
      width: 32,
      height: 32,
      borderRadius: radius.lg,
      backgroundColor: colors.accent,
      justifyContent: 'center',
      alignItems: 'center',
      marginBottom: spacing.xxs,
    },
    sendButtonDisabled: {
      backgroundColor: colors.fill,
    },
  })
);
