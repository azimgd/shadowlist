export interface InboxMessage {
  id: string;
  sender: string;
  subject: string;
  preview: string;
  receivedAt: number;
  read: boolean;
  flagged: boolean;
  avatarUrl?: string;
}
