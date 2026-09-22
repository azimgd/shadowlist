import { useCallback } from 'react';
import { useLabels } from '../labels';
import { ContactRow } from './ContactRow';
import { defaultContactsLabels, type ContactsLabels } from './labels';
import type { ContactItem } from './types';

export interface ContactRowOptions {
  onPressItem?: (item: ContactItem) => void;
  // Enables swipe-to-delete on every row.
  onDelete?: (id: string) => void;
  disclosureIndicator?: boolean;
  labels?: Partial<ContactsLabels>;
}

/*
 * Stable while the callbacks are, so ElementRenderer's per-row memoization keeps working.
 */
export function useContactRowRenderer({
  onPressItem,
  onDelete,
  disclosureIndicator,
  labels,
}: ContactRowOptions) {
  const rowLabels = useLabels(defaultContactsLabels, labels);
  return useCallback(
    ({ element }: { element: ContactItem }) => (
      <ContactRow
        item={element}
        onPress={onPressItem}
        onDelete={onDelete}
        disclosureIndicator={disclosureIndicator}
        labels={rowLabels}
      />
    ),
    [onPressItem, onDelete, disclosureIndicator, rowLabels]
  );
}
