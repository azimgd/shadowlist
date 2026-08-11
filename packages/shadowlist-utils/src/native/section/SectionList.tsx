import { forwardRef, useCallback } from 'react';
import {
  SectionList as ShadowListSectionList,
  type SectionListData,
  type SectionListProps,
  type ShadowListCommands,
} from 'shadowlist';
import type { ContactItem } from 'shadowlist-utils';
import { ContactRow } from '../contacts/ContactRow';
import { SectionHeader } from '../primitives/SectionHeader';
import { ItemSeparator } from '../primitives/ItemSeparator';

// A section carrying a display `title` (used by the default sticky header).
export type ContactSectionMeta = { title: string };

// Doesn't close over any per-render props/state, so it's hoisted to module scope
// instead of being recreated (and defeating ElementRenderer's memoization) on every
// render of the wrapper below.
function defaultRenderSectionHeader({
  section,
}: {
  section: SectionListData<ContactItem, ContactSectionMeta>;
}) {
  return <SectionHeader title={section.title} count={section.data.length} />;
}

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
  ({ renderElement, renderSectionHeader, onDelete, ...props }, ref) => {
    // Stable unless `onDelete` changes, so ElementRenderer's per-row memoization (keyed
    // on renderElement identity) isn't defeated by every re-render of this wrapper.
    const defaultRenderElement = useCallback<
      NonNullable<
        SectionListProps<ContactItem, ContactSectionMeta>['renderElement']
      >
    >(
      ({ element }) => <ContactRow element={element} onDelete={onDelete} />,
      [onDelete]
    );

    return (
      <ShadowListSectionList
        ref={ref}
        renderElement={renderElement ?? defaultRenderElement}
        renderSectionHeader={renderSectionHeader ?? defaultRenderSectionHeader}
        ItemSeparatorComponent={<ItemSeparator />}
        {...props}
      />
    );
  }
);
