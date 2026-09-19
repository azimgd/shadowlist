export interface ActivityActor {
  name: string;
  avatarUrl?: string;
  avatarColor?: string;
}

export interface ActivityItem {
  id: string;
  actor: ActivityActor;
  action: string;
  text?: string;
  createdAt: Date | number;
  read?: boolean;
}
