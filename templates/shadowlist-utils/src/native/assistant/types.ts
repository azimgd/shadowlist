import type { ColorValue } from 'react-native';

export interface AssistantAttachment {
  id: string;
  kind: 'image' | 'file';
  name: string;
  detail?: string;
  uri?: string;
  color?: ColorValue;
}

/*
 * Stopped is not failed. A tool the reader stopped was not a failure. It should not
 * show red.
 */
export type AssistantToolStatus = 'running' | 'done' | 'failed' | 'stopped';

interface AssistantToolCallBase {
  id: string;
  name: string;
  input: string;
  at?: number;
}

export interface AssistantRunningToolCall extends AssistantToolCallBase {
  status: 'running';
}

export interface AssistantDoneToolCall extends AssistantToolCallBase {
  status: 'done';
  output: string;
}

export interface AssistantFailedToolCall extends AssistantToolCallBase {
  status: 'failed';
  output?: string;
}

export interface AssistantStoppedToolCall extends AssistantToolCallBase {
  status: 'stopped';
}

export type AssistantToolCall =
  | AssistantRunningToolCall
  | AssistantDoneToolCall
  | AssistantFailedToolCall
  | AssistantStoppedToolCall;

export interface AssistantSource {
  id: string;
  title: string;
  url: string;
  domain?: string;
}

interface AssistantTurnBase {
  content: string;
  thinking?: string;
  thinkingMs?: number;
  toolCalls?: readonly AssistantToolCall[];
}

export interface AssistantStreamingTurn extends AssistantTurnBase {
  status: 'streaming';
}

export interface AssistantDoneTurn extends AssistantTurnBase {
  status: 'done';
  sources?: readonly AssistantSource[];
  followUps?: readonly string[];
}

export interface AssistantStoppedTurn extends AssistantTurnBase {
  status: 'stopped';
}

export interface AssistantFailedTurn extends AssistantTurnBase {
  status: 'failed';
  error?: string;
}

export type AssistantTurn =
  | AssistantStreamingTurn
  | AssistantDoneTurn
  | AssistantStoppedTurn
  | AssistantFailedTurn;

export type AssistantTurnStatus = AssistantTurn['status'];

export type AssistantFeedback = 'good' | 'bad';

export interface AssistantPrompt {
  id: string;
  role: 'user';
  text: string;
  attachments?: readonly AssistantAttachment[];
}

export interface AssistantReply {
  id: string;
  role: 'assistant';
  variants: readonly AssistantTurn[];
  variantIndex: number;
  model?: string;
  feedback?: AssistantFeedback;
}

export interface AssistantEndMarker {
  id: string;
  role: 'end';
}

export type AssistantMessage =
  | AssistantPrompt
  | AssistantReply
  | AssistantEndMarker;

export interface AssistantSuggestion {
  title: string;
  prompt: string;
}
