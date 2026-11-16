import { View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { Poll } from 'shadowlist-utils/native';
import { usePollQuery, useVote } from '../queries/poll';
import { QueryStatus } from './QueryStatus';
import { useScreenStyles } from './screenStyles';

export const PollScreen = () => {
  const styles = useScreenStyles();
  const insets = useSafeAreaInsets();
  const poll = usePollQuery();
  const { mutate: vote } = useVote();

  if (poll.data === undefined) {
    return <QueryStatus error={poll.error} onRetry={poll.refetch} />;
  }

  return (
    <View style={[styles.container, { paddingBottom: insets.bottom }]}>
      <Poll.List poll={poll.data} onVote={vote} style={styles.list} />
    </View>
  );
};
