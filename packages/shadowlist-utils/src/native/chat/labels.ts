export interface ChatLabels {
  image: string;
  placeholder: string;
  send: string;
  // Read by screen readers on a message still in flight.
  sending: string;
  // The line under a message that failed to send, which retries it when pressed.
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
