import { MasonryList } from './MasonryList';
import { MasonryCard } from './MasonryCard';

export type { MasonryListProps } from './MasonryList';
export type { MasonryCardProps } from './MasonryCard';

/* Masonry domain: <Masonry.List data={items} columns={3} /> + <Masonry.Card />. */
export const Masonry = {
  List: MasonryList,
  Card: MasonryCard,
};
