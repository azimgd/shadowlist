import type { ColorValue } from 'react-native';

export interface ReorderTileItem {
  id: string;
  title: string;
  label?: string;
  color?: ColorValue;
  aspectRatio?: number;
}
