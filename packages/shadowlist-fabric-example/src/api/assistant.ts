import type {
  AssistantFeedback,
  AssistantMessage,
} from 'shadowlist-utils/native';
import { buildHistory } from '../fixtures/assistant';
import { request, type RequestSignal } from './network';

// Earlier question/answer pairs per history page.
const HISTORY_PAGE_PAIRS = 3;

// Page 0 is the most recent earlier exchange; higher pages go further back.
export function fetchAssistantHistory(
  page: number,
  signal?: RequestSignal
): Promise<AssistantMessage[]> {
  return request(
    () => buildHistory(HISTORY_PAGE_PAIRS, page * HISTORY_PAGE_PAIRS),
    signal
  );
}

const feedbackByMessage = new Map<string, AssistantFeedback | undefined>();

export function sendAssistantFeedback({
  messageId,
  feedback,
}: {
  messageId: string;
  feedback: AssistantFeedback | undefined;
}): Promise<void> {
  return request(() => {
    feedbackByMessage.set(messageId, feedback);
  });
}
