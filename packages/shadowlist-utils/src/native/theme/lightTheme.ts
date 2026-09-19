import type { Theme } from './Theme';
import { baseTokens } from './baseTokens';

export const lightTheme = {
  ...baseTokens,
  colors: {
    background: '#FFFFFF',
    elevated: '#F2F2F7',
    elevated2: '#E5E5EA',
    label: '#000000',
    secondaryLabel: 'rgba(60,60,67,0.6)',
    tertiaryLabel: 'rgba(60,60,67,0.3)',
    separator: 'rgba(60,60,67,0.29)',
    fill: 'rgba(118,118,128,0.12)',
    accent: '#007AFF',
    accentSoft: 'rgba(0,122,255,0.16)',
    onAccent: '#FFFFFF',
    blue: '#007AFF',
    green: '#34C759',
    red: '#FF3B30',
    redSoft: 'rgba(255,59,48,0.16)',
    avatarPalette: [
      '#FF6B6B',
      '#4ECDC4',
      '#45B7D1',
      '#FFA07A',
      '#98D8C8',
      '#F7DC6F',
      '#BB8FCE',
      '#85C1E2',
      '#F8B195',
      '#C06C84',
    ],
  },
} as const satisfies Theme;
