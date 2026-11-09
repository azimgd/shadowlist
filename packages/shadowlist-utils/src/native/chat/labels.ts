export interface ChatLabels {
  image: string;
  placeholder: string;
  send: string;
  sending: string;
  failed: string;
  retryHint: string;
}

export const defaultChatLabels: ChatLabels = {
  image: 'Image',
  placeholder: 'Message',
  send: 'Send',
  sending: 'Sending',
  failed: 'Not delivered. Tap to retry.',
  retryHint: 'Sends the message again',
};
