import { SectionList as SectionListComponent } from './SectionList';
import { ContactRow } from '../contacts/ContactRow';
import { SectionHeader } from '../primitives/SectionHeader';

export type { SectionListProps_, ContactSectionMeta } from './SectionList';

// SectionList domain: <SectionList.List sections={...} /> + .Row + .SectionHeader.
export const SectionList = {
  List: SectionListComponent,
  Row: ContactRow,
  SectionHeader: SectionHeader,
};
