import type { ViewStyle } from 'react-native';

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

let createdCards = 0;

/*
 * Widths vary per card. The style object is built once so rows never allocate one.
 */
export function createCarouselCards(
  count: number,
  prefix: string
): CarouselCard[] {
  return Array.from({ length: count }, () => {
    const number = createdCards++;
    return {
      id: `card-${number}`,
      label: `${prefix} ${number}`,
      style: {
        width: 110 + ((number * 37) % 150),
        backgroundColor: CARD_COLORS[number % CARD_COLORS.length]!,
      },
    };
  });
}
