import type { ColorValue } from 'react-native';

export interface ContactItem {
  id: string;
  name: string;
  subtitle?: string;
  avatarUrl?: string;
  avatarColor?: ColorValue;
}
