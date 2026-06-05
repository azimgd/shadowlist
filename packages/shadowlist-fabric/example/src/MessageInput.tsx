import { useState } from 'react';
import {
  View,
  TextInput,
  TouchableOpacity,
  StyleSheet,
  KeyboardAvoidingView,
  Platform,
} from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

interface MessageInputProps {
  onSend: (message: string) => void;
}

export const MessageInput = ({ onSend }: MessageInputProps) => {
  const [message, setMessage] = useState('');
  const insets = useSafeAreaInsets();

  const handleSend = () => {
    if (message.trim()) {
      onSend(message.trim());
      setMessage('');
    }
  };

  return (
    <KeyboardAvoidingView
      behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
      keyboardVerticalOffset={0}
    >
      <View style={[styles.container, { paddingBottom: insets.bottom || 8 }]}>
        <View style={styles.inputContainer}>
          <TextInput
            style={styles.input}
            value={message}
            onChangeText={setMessage}
            placeholder="iMessage"
            placeholderTextColor="#8E8E93"
            multiline
            maxLength={500}
          />
        </View>
        <TouchableOpacity
          style={[
            styles.sendButton,
            !message.trim() && styles.sendButtonDisabled,
          ]}
          onPress={handleSend}
          disabled={!message.trim()}
        />
      </View>
    </KeyboardAvoidingView>
  );
};

const styles = StyleSheet.create({
  container: {
    flexDirection: 'row',
    alignItems: 'flex-end',
    paddingHorizontal: 8,
    paddingTop: 8,
    backgroundColor: '#1C1C1E',
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: '#38383A',
  },
  inputContainer: {
    flex: 1,
    backgroundColor: '#2C2C2E',
    borderRadius: 20,
    paddingHorizontal: 12,
    paddingVertical: 6,
    marginRight: 8,
    minHeight: 32,
    maxHeight: 100,
  },
  input: {
    color: '#FFFFFF',
    fontSize: 16,
    lineHeight: 20,
    padding: 0,
    margin: 0,
  },
  sendButton: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: '#34C759',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 0,
  },
  sendButtonDisabled: {
    backgroundColor: '#2C2C2E',
  },
});
