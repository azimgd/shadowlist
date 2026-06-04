import { useState, type CSSProperties } from 'react';

interface MessageInputProps {
  onSend: (message: string) => void;
}

export const MessageInput = ({ onSend }: MessageInputProps) => {
  const [message, setMessage] = useState('');

  const handleSend = () => {
    if (message.trim()) {
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
          placeholder="iMessage"
          maxLength={500}
          rows={1}
        />
      </div>
      <button
        type="button"
        style={{
          ...styles.sendButton,
          ...(message.trim() ? null : styles.sendButtonDisabled),
        }}
        onClick={handleSend}
        disabled={!message.trim()}
        aria-label="Send"
      />
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
    background: '#1C1C1E',
    borderTop: '1px solid #38383A',
  },
  inputContainer: {
    flex: 1,
    display: 'flex',
    background: '#2C2C2E',
    borderRadius: 20,
    padding: '6px 12px',
    marginRight: 8,
    minHeight: 32,
    maxHeight: 100,
  },
  input: {
    flex: 1,
    color: '#FFFFFF',
    fontSize: 16,
    lineHeight: '20px',
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
    background: '#34C759',
    cursor: 'pointer',
    flexShrink: 0,
  },
  sendButtonDisabled: {
    background: '#2C2C2E',
    cursor: 'default',
  },
};
