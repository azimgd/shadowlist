import { useState, type CSSProperties } from 'react';
import { colors, typography, radius } from '../theme';
import { ArrowUp } from '../icons';

export interface ChatInputProps {
  onSend: (message: string) => void;
  placeholder?: string;
}

// Composer bar; Enter sends, Shift+Enter inserts a newline.
export const ChatInput = ({ onSend, placeholder = 'iMessage' }: ChatInputProps) => {
  const [message, setMessage] = useState('');
  const canSend = message.trim().length > 0;

  const handleSend = () => {
    if (canSend) {
      onSend(message.trim());
      setMessage('');
    }
  };

  return (
    <div style={styles.container}>
      <div style={styles.inputContainer}>
        <textarea
          style={styles.input}
          value={message}
          onChange={(event) => setMessage(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === 'Enter' && !event.shiftKey) {
              event.preventDefault();
              handleSend();
            }
          }}
          placeholder={placeholder}
          maxLength={500}
          rows={1}
        />
      </div>
      <button
        type="button"
        style={{ ...styles.sendButton, ...(canSend ? null : styles.sendButtonDisabled) }}
        onClick={handleSend}
        disabled={!canSend}
        aria-label="Send"
      >
        <ArrowUp size={20} color={canSend ? colors.label : colors.secondaryLabel} strokeWidth={2.4} />
      </button>
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  container: {
    flexShrink: 0,
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'flex-end',
    padding: '8px 8px',
    background: colors.background,
    borderTop: `1px solid ${colors.separator}`,
  },
  inputContainer: {
    flex: 1,
    display: 'flex',
    background: colors.background,
    border: `1px solid ${colors.separator}`,
    borderRadius: radius.lg + 2,
    padding: '7px 12px',
    marginRight: 8,
    minHeight: 36,
    maxHeight: 120,
    boxSizing: 'border-box',
  },
  input: {
    flex: 1,
    color: colors.label,
    ...typography.body,
    padding: 0,
    margin: 0,
    border: 'none',
    outline: 'none',
    background: 'transparent',
    resize: 'none',
    fontFamily: 'inherit',
  },
  sendButton: {
    appearance: 'none',
    border: 'none',
    width: 32,
    height: 32,
    borderRadius: 16,
    background: colors.accent,
    cursor: 'pointer',
    flexShrink: 0,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
  },
  sendButtonDisabled: {
    background: colors.fill,
    cursor: 'default',
  },
};
