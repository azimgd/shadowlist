import type { AssistantToolStatus } from './types';

export interface AssistantLabels {
  assistantName: string;
  copy: string;
  copied: string;
  copyCode: string;
  codeLanguageFallback: string;
  share: string;
  regenerate: string;
  retry: string;
  goodResponse: string;
  badResponse: string;
  previousVersion: string;
  nextVersion: string;
  versionPosition: (position: number, count: number) => string;
  responseStopped: string;
  responseFailed: string;
  source: (position: number, title: string) => string;
  edit: string;
  showActions: string;
  showActionsHint: string;
  thinking: string;
  thoughtFor: (seconds: number) => string;
  toolStatus: (status: AssistantToolStatus) => string;
  toolInput: string;
  toolOutput: string;
  typing: string;
  scrollToLatest: string;
  placeholder: string;
  send: string;
  stop: string;
  addAttachment: string;
  removeAttachment: (name: string) => string;
  editingMessage: string;
  cancelEditing: string;
  model: (name: string) => string;
  thinkingToggle: string;
  thinkingToggleDescription: string;
  emptyTitle: string;
  emptySubtitle: string;
}

const TOOL_STATUS: Record<AssistantToolStatus, string> = {
  running: 'Running',
  done: 'Done',
  failed: 'Failed',
  stopped: 'Stopped',
};

export const defaultAssistantLabels: AssistantLabels = {
  assistantName: 'Assistant',
  copy: 'Copy',
  copied: 'Copied',
  copyCode: 'Copy code',
  codeLanguageFallback: 'code',
  share: 'Share',
  regenerate: 'Regenerate',
  retry: 'Retry',
  goodResponse: 'Good response',
  badResponse: 'Bad response',
  previousVersion: 'Previous version',
  nextVersion: 'Next version',
  versionPosition: (position, count) => `${position} / ${count}`,
  responseStopped: 'Response stopped',
  responseFailed: 'Something went wrong.',
  source: (position, title) => `Source ${position}: ${title}`,
  edit: 'Edit',
  showActions: 'Show actions',
  showActionsHint: 'Long press for copy and edit',
  thinking: 'Thinking',
  thoughtFor: (seconds) => `Thought for ${seconds}s`,
  toolStatus: (status) => TOOL_STATUS[status],
  toolInput: 'Input',
  toolOutput: 'Output',
  typing: 'Assistant is typing',
  scrollToLatest: 'Scroll to latest',
  placeholder: 'Ask anything',
  send: 'Send',
  stop: 'Stop generating',
  addAttachment: 'Add attachment',
  removeAttachment: (name) => `Remove ${name}`,
  editingMessage: 'Editing message',
  cancelEditing: 'Cancel editing',
  model: (name) => `Model: ${name}`,
  thinkingToggle: 'Think',
  thinkingToggleDescription: 'Extended thinking',
  emptyTitle: 'How can I help?',
  emptySubtitle: '',
};
