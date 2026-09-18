import type { IconComponent } from '../icons';

export interface PollOption {
  id: string;
  label: string;
  votes: number;
  icon?: IconComponent;
}

export interface PollData {
  question: string;
  options: ReadonlyArray<PollOption>;
  selectedId?: string;
}
