export interface RelativeTime {
  unit: 'now' | 'minutes' | 'hours' | 'days' | 'date';
  count: number;
}
