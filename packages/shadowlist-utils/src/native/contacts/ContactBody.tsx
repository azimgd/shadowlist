import { memo } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { Avatar } from '../primitives/Avatar';
import { createStyles } from '../theme';
import type { ContactItem } from './types';

export interface ContactBodyProps {
  contact: ContactItem;
  avatarStyle?: StyleProp<ViewStyle>;
}

export const ContactBody = memo(
  ({ contact, avatarStyle }: ContactBodyProps) => {
    const styles = useStyles();
    return (
      <>
        <Avatar
          name={contact.name}
          uri={contact.avatarUrl}
          color={contact.avatarColor}
          style={[styles.avatar, avatarStyle]}
        />
        <View style={styles.text}>
          <Text style={styles.name} numberOfLines={1}>
            {contact.name}
          </Text>
          {contact.subtitle ? (
            <Text style={styles.subtitle} numberOfLines={1}>
              {contact.subtitle}
            </Text>
          ) : null}
        </View>
      </>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    avatar: {
      marginRight: theme.spacing.md,
    },
    text: {
      flex: 1,
    },
    name: {
      color: theme.colors.label,
      ...theme.typography.body,
    },
    subtitle: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
      marginTop: 1,
    },
  })
);
