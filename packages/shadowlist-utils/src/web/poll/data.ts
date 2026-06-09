import type { ComponentType } from 'react';
import { buildPollOption } from 'shadowlist-utils';
import { Bell, CircleSlash, Globe, HalfCircle, Swatch } from '../icons';

export type IconComponent = ComponentType<{ size?: number; color?: string }>;

export type PollOption = {
  id: string;
  Icon: IconComponent;
  label: string;
  votes: number;
};

/*
 * Only the icon binding is platform-specific; labels, votes and ids come from the
 * agnostic root (buildPollOption). Icons are in POLL_OPTION_LABELS order.
 */
const OPTION_ICONS: IconComponent[] = [
  HalfCircle,
  CircleSlash,
  Bell,
  Globe,
  Swatch,
];

export const buildOption = (index: number): PollOption =>
  buildPollOption(index, OPTION_ICONS);

export const buildPoll = (length = 5): PollOption[] =>
  Array.from({ length }, (_, index) => buildOption(index));
