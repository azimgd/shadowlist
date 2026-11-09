export interface InboxLabels {
  read: string;
  unread: string;
  markRead: string;
  markUnread: string;
  flag: string;
  unflag: string;
  delete: string;
  reply: string;
  select: string;
  selected: string;
  unreadState: string;
  flaggedState: string;
}

export const defaultInboxLabels: InboxLabels = {
  read: 'Read',
  unread: 'Unread',
  markRead: 'Mark as Read',
  markUnread: 'Mark as Unread',
  flag: 'Flag',
  unflag: 'Unflag',
  delete: 'Delete',
  reply: 'Reply',
  select: 'Select',
  selected: 'Selected',
  unreadState: 'Unread',
  flaggedState: 'Flagged',
};
