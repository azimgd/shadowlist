import { memo, useCallback } from 'react';
import type { StyleProp, ViewStyle } from 'react-native';
import { ContactRowContent } from './ContactRowContent';
import type { ContactsLabels } from './labels';
import { SwipeableContactRow } from './SwipeableContactRow';
import type { ContactItem } from './types';

export interface ContactRowProps {
  item: ContactItem;
  onPress?: (item: ContactItem) => void;
  onDelete?: (id: string) => void;
  disclosureIndicator?: boolean;
  labels?: Partial<ContactsLabels>;
  style?: StyleProp<ViewStyle>;
  avatarStyle?: StyleProp<ViewStyle>;
}

const StaticContactRow = ({
  item,
  onPress,
  disclosureIndicator = true,
  style,
  avatarStyle,
}: Omit<ContactRowProps, 'onDelete' | 'labels'>) => {
  const handlePress = useCallback(() => onPress?.(item), [onPress, item]);
  return (
    <ContactRowContent
      item={item}
      onPress={onPress !== undefined ? handlePress : undefined}
      isPressable={onPress !== undefined}
      disclosureIndicator={disclosureIndicator}
      style={style}
      avatarStyle={avatarStyle}
    />
  );
};

export const ContactRow = memo((props: ContactRowProps) =>
  props.onDelete !== undefined ? (
    <SwipeableContactRow {...props} onDelete={props.onDelete} />
  ) : (
    <StaticContactRow
      item={props.item}
      onPress={props.onPress}
      disclosureIndicator={props.disclosureIndicator}
      style={props.style}
      avatarStyle={props.avatarStyle}
    />
  )
);
