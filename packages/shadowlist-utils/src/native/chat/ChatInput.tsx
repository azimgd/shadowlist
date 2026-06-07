import { useState } from 'react';
import { View, TextInput, Pressable, StyleSheet } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { colors, typography, radius } from '../theme';
import { ArrowUp } from '../icons';

export interface ChatInputProps {
  onSend: (message: string) => void;
  placeholder?: string;
}

// Composer bar with safe-area bottom padding.
export const ChatInput = ({
  onSend,
  placeholder = 'iMessage',
}: ChatInputProps) => {
  const [message, setMessage] = useState('');
  const insets = useSafeAreaInsets();
  const canSend = message.trim().length > 0;

  const handleSend = () => {
    if (canSend) {
      onSend(message.trim());
      setMessage('');
    }
  };

  return (
    <View style={[styles.container, { paddingBottom: insets.bottom || 8 }]}>
      <View style={styles.inputContainer}>
        <TextInput
          style={styles.input}
          value={message}
          onChangeText={setMessage}
          placeholder={placeholder}
          placeholderTextColor={colors.secondaryLabel}
          multiline
          maxLength={500}
        />
      </View>
      <Pressable
        style={[styles.sendButton, !canSend && styles.sendButtonDisabled]}
        onPress={handleSend}
        disabled={!canSend}
      >
        <ArrowUp
          size={20}
          color={canSend ? colors.label : colors.secondaryLabel}
          weight={2.4}
        />
      </Pressable>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flexDirection: 'row',
    alignItems: 'flex-end',
    paddingHorizontal: 8,
    paddingTop: 8,
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
    paddingHorizontal: 12,
    paddingVertical: 7,
    marginRight: 8,
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
    borderRadius: 16,
    backgroundColor: colors.accent,
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 2,
  },
  sendButtonDisabled: {
    backgroundColor: colors.fill,
  },
});
