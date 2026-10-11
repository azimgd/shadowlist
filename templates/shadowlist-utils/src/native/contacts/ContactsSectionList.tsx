import { forwardRef } from 'react';
import {
  SectionList,
  type SectionListData,
  type SectionListCommands,
  type SectionListProps,
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

/*
 * Kept at module scope. Every row includes the separator. A new React element each render would
 * rebuild every mounted row.
 */
const ITEM_SEPARATOR = <ItemSeparator />;

const renderContactSectionHeader = ({
  section,
}: {
  section: ContactSection;
}) => <SectionHeader title={section.title} count={section.data.length} />;

export const ContactsSectionList = forwardRef<
  SectionListCommands,
  ContactsSectionListProps
>(
  (
    {
      renderItem,
      onPressItem,
      onDelete,
      disclosureIndicator,
      labels,
      ...props
    },
    ref
  ) => {
    const renderContactRow = useContactRowRenderer({
      onPressItem,
      onDelete,
      disclosureIndicator,
      labels,
    });
    return (
      <SectionList
        ref={ref}
        renderItem={renderItem ?? renderContactRow}
        renderSectionHeader={renderContactSectionHeader}
        ItemSeparatorComponent={ITEM_SEPARATOR}
        {...props}
      />
    );
  }
);
