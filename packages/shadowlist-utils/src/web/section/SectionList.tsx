import { forwardRef } from 'react';
import {
  SectionList as ShadowlistSectionList,
  type SectionListProps,
  type ShadowlistCommands,
} from 'shadowlist-wasm';
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
  // Forwarded to each contact row's swipe-to-delete button.
  onDelete?: (id: string) => void;
};

// A grouped, sticky-section list of contact rows.
export const SectionList = forwardRef<ShadowlistCommands, SectionListProps_>(
  ({ renderElement, renderSectionHeader, onDelete, ...props }, ref) => (
    <ShadowlistSectionList
      ref={ref}
      renderElement={
        renderElement ??
        (({ element, index }) => (
          <ContactRow element={element} index={index} onDelete={onDelete} />
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
