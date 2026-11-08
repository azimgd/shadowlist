import type { ReactNode } from 'react';
import { StyleSheet, View, type StyleProp, type ViewStyle } from 'react-native';
import { createStyles } from '../theme';
import { GroupedCaption } from './GroupedCaption';
import { GroupedCard } from './GroupedCard';

export interface GroupedSectionProps {
  title?: string;
  children: ReactNode;
  style?: StyleProp<ViewStyle>;
}

/*
 * One group of rows: an optional caption over a card.
 */
export const GroupedSection = ({
  title,
  children,
  style,
}: GroupedSectionProps) => {
  const styles = useStyles();
  return (
    <View style={[styles.section, style]}>
      {title !== undefined && title !== '' ? (
        <GroupedCaption title={title} />
      ) : null}
      <GroupedCard>{children}</GroupedCard>
    </View>
  );
};

const useStyles = createStyles(({ spacing }) =>
  StyleSheet.create({
    section: {
      paddingBottom: spacing.xl,
    },
  })
);
