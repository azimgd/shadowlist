import type { PollOption } from './PollOption.js';

export interface PollData {
  question: string;
  options: PollOption[];
  selectedKey?: string;
}
