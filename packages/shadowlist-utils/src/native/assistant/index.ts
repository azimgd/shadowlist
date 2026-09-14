import { AssistantList } from './AssistantList';
import { AssistantReplyMessage } from './AssistantReplyMessage';
import { AssistantUserMessage } from './AssistantUserMessage';
import { AssistantComposer } from './AssistantComposer';
import { AssistantEmpty } from './AssistantEmpty';
import { AssistantScrollButton } from './AssistantScrollButton';
import { AssistantMarkdown } from './AssistantMarkdown';
import { AssistantThinking } from './AssistantThinking';
import { AssistantToolCallCard } from './AssistantToolCallCard';
import { AssistantAttachmentChip } from './AssistantAttachmentChip';
import { AssistantTypingIndicator } from './AssistantTypingIndicator';

export type { AssistantListProps } from './AssistantList';
export type { AssistantReplyMessageProps } from './AssistantReplyMessage';
export type { AssistantUserMessageProps } from './AssistantUserMessage';
export type {
  AssistantComposerProps,
  AssistantComposerHandle,
} from './AssistantComposer';
export type { AssistantEmptyProps } from './AssistantEmpty';
export type { AssistantScrollButtonProps } from './AssistantScrollButton';
export type { AssistantMarkdownProps } from './AssistantMarkdown';
export type { AssistantThinkingProps } from './AssistantThinking';
export type { AssistantToolCallCardProps } from './AssistantToolCallCard';
export type { AssistantAttachmentChipProps } from './AssistantAttachmentChip';

export { createStreamStore, playScript } from './stream';
export type { AssistantStreamStore, StreamHandle } from './stream';
export {
  ASSISTANT_END_ID,
  ASSISTANT_END_MARKER,
  ASSISTANT_MODELS,
  ASSISTANT_SUGGESTIONS,
  emptyTurn,
  buildUserMessage,
  buildReply,
  buildHistory,
  pickScript,
} from './data';
export type {
  AssistantMessage,
  AssistantPrompt,
  AssistantReply,
  AssistantEndMarker,
  AssistantTurn,
  AssistantTurnStatus,
  AssistantToolInvocation,
  AssistantToolStatus,
  AssistantSource,
  AssistantAttachment,
  AssistantFeedback,
  AssistantScript,
} from './data';

export const Assistant = {
  List: AssistantList,
  ReplyMessage: AssistantReplyMessage,
  UserMessage: AssistantUserMessage,
  Composer: AssistantComposer,
  Empty: AssistantEmpty,
  ScrollButton: AssistantScrollButton,
  Markdown: AssistantMarkdown,
  Thinking: AssistantThinking,
  ToolCallCard: AssistantToolCallCard,
  AttachmentChip: AssistantAttachmentChip,
  TypingIndicator: AssistantTypingIndicator,
};
