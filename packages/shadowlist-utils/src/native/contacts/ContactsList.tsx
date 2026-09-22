import { forwardRef } from 'react';
import {
  ShadowList,
  type ShadowListCommands,
  type ShadowListProps,
} from 'shadowlist';
import type { ContactItem } from './types';
import {
  useContactRowRenderer,
  type ContactRowOptions,
} from './useContactRowRenderer';

export type ContactsListProps = Omit<
  ShadowListProps<ContactItem>,
  'renderElement'
> &
  ContactRowOptions & {
    renderElement?: ShadowListProps<ContactItem>['renderElement'];
  };

export const ContactsList = forwardRef<ShadowListCommands, ContactsListProps>(
  (
    {
      renderElement,
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
      <ShadowList
        ref={ref}
        renderElement={renderElement ?? renderContactRow}
        {...props}
      />
    );
  }
);
