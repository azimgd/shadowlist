import type { ColorValue } from 'react-native';

export interface ActivityActor {
  name: string;
  avatarUrl?: string;
  avatarColor?: ColorValue;
}

export interface ActivityItem {
  id: string;
  actor: ActivityActor;
  action: string;
  text?: string;
  createdAt: Date | number;
  read?: boolean;
}
