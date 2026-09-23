import { createContext, useContext, type ReactElement } from 'react';

/*
 * List wide state that rows show, like whether a reply is streaming. AssistantList provides
 * it here instead of through renderElement, so a change re-renders only the small parts
 * below that read it, not every mounted row. A reply start and end used to rebuild every row
 * twice. Outside an AssistantList the defaults change nothing.
 */
export interface AssistantRowState {
  // A reply is streaming, so Regenerate, Retry and Edit are off.
  busy: boolean;
  // The newest reply, the only one that shows follow ups.
  latestId: string | undefined;
}

const DEFAULT_ROW_STATE: AssistantRowState = {
  busy: false,
  latestId: undefined,
};

export const AssistantRowStateContext =
  createContext<AssistantRowState>(DEFAULT_ROW_STATE);

/*
 * Renders children with busy from the prop or the list. Keep the part that needs it small,
 * since this re-renders when streaming starts or ends.
 */
export function AssistantBusy({
  busy,
  children,
}: {
  busy: boolean;
  children: (busy: boolean) => ReactElement | null;
}) {
  const state = useContext(AssistantRowStateContext);
  return children(busy || state.busy);
}

/*
 * Renders children with whether this reply is the newest, from the prop or the list.
 */
export function AssistantLatest({
  messageId,
  isLatest,
  children,
}: {
  messageId: string;
  isLatest: boolean;
  children: (isLatest: boolean) => ReactElement | null;
}) {
  const state = useContext(AssistantRowStateContext);
  return children(isLatest || state.latestId === messageId);
}
