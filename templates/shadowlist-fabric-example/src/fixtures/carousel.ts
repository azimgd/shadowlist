import type { ViewStyle } from 'react-native';
import { DEBUG } from '../launchSettings';

export interface CarouselCard {
  id: string;
  label: string;
  style: ViewStyle;
}

const CARD_COLORS = [
  '#2F6FEB',
  '#8250DF',
  '#BF3989',
  '#D4A72C',
  '#1A7F37',
  '#CF222E',
];

const CITIES = [
  'Lisbon',
  'Reykjavik',
  'Kyoto',
  'Marrakesh',
  'Tromsø',
  'Oaxaca',
  'Tbilisi',
  'Hobart',
  'Porto',
  'Seoul',
  'Cusco',
  'Valletta',
];

let createdCards = 0;

/*
 * Widths vary per card. The style object is built once so rows never allocate one.
 */
export function createCarouselCards(count: number): CarouselCard[] {
  return Array.from({ length: count }, () => {
    const number = createdCards++;
    const city = CITIES[number % CITIES.length]!;
    const fare = 89 + ((number * 53) % 400);
    const prefix = DEBUG ? `#${number} ` : '';
    return {
      id: `card-${number}`,
      label: `${prefix}${city}\nfrom $${fare}`,
      style: {
        width: 110 + ((number * 37) % 150),
        backgroundColor: CARD_COLORS[number % CARD_COLORS.length]!,
      },
    };
  });
}
