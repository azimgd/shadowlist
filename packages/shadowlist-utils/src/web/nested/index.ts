import { NestedList } from './NestedList';
import { NestedRow } from './NestedRow';
import { NestedCard } from './NestedCard';

export type { NestedListProps } from './NestedList';
export type { NestedRowProps } from './NestedRow';
export type { NestedCardProps } from './NestedCard';

// Nested domain: <Nested.List /> of <Nested.Row /> carousels of <Nested.Card />.
export const Nested = {
  List: NestedList,
  Row: NestedRow,
  Card: NestedCard,
};
