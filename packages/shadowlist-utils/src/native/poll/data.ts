import type { ComponentType } from 'react';
import { generateUniqueId } from 'shadowlist-utils';
import { Bell, CircleSlash, Globe, HalfCircle, Swatch } from '../icons';

export type IconComponent = ComponentType<{ size?: number; color?: string }>;

export interface PollOption {
  id: string;
  Icon: IconComponent;
  label: string;
  votes: number;
}

export const OPTION_SEEDS: { Icon: IconComponent; label: string }[] = [
  { Icon: HalfCircle, label: 'Dark mode' },
  { Icon: CircleSlash, label: 'Offline mode' },
  { Icon: Bell, label: 'Notifications' },
  { Icon: Globe, label: 'Translations' },
  { Icon: Swatch, label: 'Custom themes' },
];

// One option, cycling the seed set; a fresh id keeps prepend/append unique.
export function buildOption(index: number): PollOption {
  const seed = OPTION_SEEDS[index % OPTION_SEEDS.length]!;
  return {
    id: generateUniqueId(),
    Icon: seed.Icon,
    label: seed.label,
    votes: 5 + Math.floor(Math.random() * 40),
  };
}

export function buildPoll(length = 5): PollOption[] {
  return Array.from({ length }, (_, index) => buildOption(index));
}
