import { memo } from 'react';
import {
  View,
  Text,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles } from '../theme';
import { defaultPollLabels, type PollLabels } from './labels';

export interface PollFooterProps {
  totalVotes: number;
  labels?: Partial<PollLabels>;
  style?: StyleProp<ViewStyle>;
}

export const PollFooter = memo(
  ({ totalVotes, labels, style }: PollFooterProps) => {
    const styles = useStyles();
    const l = useLabels(defaultPollLabels, labels);
    return (
      <View style={[styles.container, style]}>
        <Text style={styles.text}>{l.totalVotes(totalVotes)}</Text>
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      width: '100%',
      alignItems: 'center',
      backgroundColor: theme.colors.background,
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: theme.spacing.lg,
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: theme.colors.separator,
    },
    text: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.footnote,
    },
  })
);
