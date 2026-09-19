export interface AssistantAttachment {
  id: string;
  kind: 'image' | 'file';
  name: string;
  // Secondary line under the name, e.g. a file size.
  detail?: string;
  // Image preview. Without one, image chips show `color` (or a neutral fill).
  uri?: string;
  color?: string;
}

/*
 * 'stopped' is distinct from 'failed' on purpose: a tool still running when the reader hits
 * stop was interrupted by them, not by a failure, and painting it red says the opposite.
 */
export type AssistantToolStatus = 'running' | 'done' | 'failed' | 'stopped';

interface AssistantToolCallBase {
  id: string;
  name: string;
  input: string;
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
  // Shown under the title; derived from `url` when omitted.
  domain?: string;
}

interface AssistantTurnBase {
  content: string;
  thinking?: string;
  // Set once reasoning has ended; undefined while the model is still thinking.
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
  // Versions of this reply; regenerating adds one, the pager moves between them.
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
