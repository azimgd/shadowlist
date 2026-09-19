import type { Theme } from './Theme';
import { baseTokens } from './baseTokens';

export const darkTheme = {
  ...baseTokens,
  colors: {
    background: '#000000',
    elevated: '#1C1C1E',
    elevated2: '#2C2C2E',
    label: '#FFFFFF',
    secondaryLabel: 'rgba(235,235,245,0.6)',
    tertiaryLabel: 'rgba(235,235,245,0.3)',
    separator: 'rgba(84,84,88,0.6)',
    fill: 'rgba(118,118,128,0.24)',
    accent: '#0A84FF',
    accentSoft: 'rgba(10,132,255,0.16)',
    onAccent: '#FFFFFF',
    blue: '#0A84FF',
    green: '#30D158',
    red: '#FF453B',
    redSoft: 'rgba(255,69,59,0.16)',
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
