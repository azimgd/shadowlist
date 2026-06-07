import { forwardRef } from 'react';
import {
  SectionList as ShadowlistSectionList,
  type SectionListProps,
  type ShadowlistCommands,
} from 'shadowlist';
import type { ContactItem } from 'shadowlist-utils';
import { ContactRow } from '../contacts/ContactRow';
import { SectionHeader } from '../primitives/SectionHeader';
import { ItemSeparator } from '../primitives/ItemSeparator';

// A section carrying a display `title` (used by the default sticky header).
export type ContactSectionMeta = { title: string };

export type SectionListProps_ = Omit<
  SectionListProps<ContactItem, ContactSectionMeta>,
  'renderItem' | 'renderSectionHeader'
> & {
  renderItem?: SectionListProps<ContactItem, ContactSectionMeta>['renderItem'];
  renderSectionHeader?: SectionListProps<
    ContactItem,
    ContactSectionMeta
  >['renderSectionHeader'];
  // Forwarded to each contact row's swipe-to-delete button.
  onDelete?: (id: string) => void;
};

/*
 * A grouped, sticky-section list of contact rows. Pass `sections`
 * (`{ title, data }[]`) to get sticky A–Z headers, inset separators and
 * swipe-to-delete out of the box. Override `renderItem`/`renderSectionHeader`
 * to repurpose it.
 */
export const SectionList = forwardRef<ShadowlistCommands, SectionListProps_>(
  ({ renderItem, renderSectionHeader, onDelete, ...props }, ref) => (
    <ShadowlistSectionList
      ref={ref}
      renderItem={
        renderItem ??
        (({ item, index }) => (
          <ContactRow element={item} index={index} onDelete={onDelete} />
        ))
      }
      renderSectionHeader={
        renderSectionHeader ??
        (({ section }) => (
          <SectionHeader title={section.title} count={section.data.length} />
        ))
      }
      ItemSeparatorComponent={<ItemSeparator />}
      {...props}
    />
  )
);
