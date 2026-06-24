import { forwardRef } from 'react';
import {
  SectionList as ShadowListSectionList,
  type SectionListProps,
  type ShadowListCommands,
} from 'shadowlist';
import type { ContactItem } from 'shadowlist-utils';
import { ContactRow } from '../contacts/ContactRow';
import { SectionHeader } from '../primitives/SectionHeader';
import { ItemSeparator } from '../primitives/ItemSeparator';

// A section carrying a display `title` (used by the default sticky header).
export type ContactSectionMeta = { title: string };

export type SectionListProps_ = Omit<
  SectionListProps<ContactItem, ContactSectionMeta>,
  'renderElement' | 'renderSectionHeader'
> & {
  renderElement?: SectionListProps<
    ContactItem,
    ContactSectionMeta
  >['renderElement'];
  renderSectionHeader?: SectionListProps<
    ContactItem,
    ContactSectionMeta
  >['renderSectionHeader'];
  // Forwarded to each contact row's swipe-to-delete button;
  onDelete?: (key: string) => void;
};

/*
 * A grouped, sticky-section list of contact rows. Pass `sections`
 * (`{ title, data }[]`) to get sticky A to Z headers, inset separators and
 * swipe-to-delete out of the box. Override `renderElement`/`renderSectionHeader`
 * to repurpose it.
 */
export const SectionList = forwardRef<ShadowListCommands, SectionListProps_>(
  ({ renderElement, renderSectionHeader, onDelete, ...props }, ref) => (
    <ShadowListSectionList
      ref={ref}
      renderElement={
        renderElement ??
        (({ element }) => <ContactRow element={element} onDelete={onDelete} />)
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
