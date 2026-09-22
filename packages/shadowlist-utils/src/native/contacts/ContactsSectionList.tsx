import { forwardRef } from 'react';
import {
  SectionList,
  type SectionListData,
  type SectionListProps,
  type ShadowListCommands,
} from 'shadowlist';
import { ItemSeparator } from '../primitives/ItemSeparator';
import { SectionHeader } from '../primitives/SectionHeader';
import type { ContactItem } from './types';
import {
  useContactRowRenderer,
  type ContactRowOptions,
} from './useContactRowRenderer';

export interface ContactSectionMeta {
  title: string;
}

export type ContactSection = SectionListData<ContactItem, ContactSectionMeta>;

export type ContactsSectionListProps = SectionListProps<
  ContactItem,
  ContactSectionMeta
> &
  ContactRowOptions;

// Kept at module scope. Every row includes the separator, so a new element each render would rebuild every mounted row.
const ITEM_SEPARATOR = <ItemSeparator />;

const renderContactSectionHeader = ({
  section,
}: {
  section: ContactSection;
}) => <SectionHeader title={section.title} count={section.data.length} />;

export const ContactsSectionList = forwardRef<
  ShadowListCommands,
  ContactsSectionListProps
>(({ renderElement, onPressItem, onDelete, labels, ...props }, ref) => {
  const renderContactRow = useContactRowRenderer({
    onPressItem,
    onDelete,
    labels,
  });
  return (
    <SectionList
      ref={ref}
      renderElement={renderElement ?? renderContactRow}
      renderSectionHeader={renderContactSectionHeader}
      ItemSeparatorComponent={ITEM_SEPARATOR}
      {...props}
    />
  );
});
