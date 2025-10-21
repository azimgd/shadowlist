import { useEffect, useRef, useMemo, memo } from 'react';
import { View, Text, StyleSheet, Animated, Easing } from 'react-native';

interface HeavyListItemProps {
  index: number;
  text: string;
}

export const HeavyListItem = memo(({ index, text }: HeavyListItemProps) => {
  const spinValue = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    const spin = Animated.loop(
      Animated.timing(spinValue, {
        toValue: 1,
        duration: 2000,
        easing: Easing.linear,
        useNativeDriver: true,
      })
    );

    spin.start();

    return () => spin.stop();
  }, [spinValue]);

  const rotate = useMemo(
    () =>
      spinValue.interpolate({
        inputRange: [0, 1],
        outputRange: ['0deg', '360deg'],
      }),
    [spinValue]
  );

  const animatedStyle = useMemo(
    () => [styles.avatar, { transform: [{ rotate }] }],
    [rotate]
  );

  return (
    <View style={styles.container}>
      <Animated.View style={animatedStyle}>
        <View style={styles.avatarInner} />
      </Animated.View>

      <View style={styles.content}>
        <Text style={styles.username}>User {index}</Text>
        <Text style={styles.handle}>@user{index}</Text>
        <Text style={styles.text}>{text}</Text>
      </View>
    </View>
  );
});

const styles = StyleSheet.create({
  container: {
    flexDirection: 'row',
    padding: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#2f3336',
    backgroundColor: '#333333',
  },
  avatar: {
    width: 48,
    height: 48,
    borderRadius: 16,
    backgroundColor: '#1d9bf0',
    marginRight: 12,
    justifyContent: 'center',
    alignItems: 'center',
  },
  avatarInner: {
    width: 24,
    height: 24,
    borderRadius: 12,
    backgroundColor: '#ffffff',
  },
  content: {
    flex: 1,
  },
  username: {
    color: '#ffffff',
    fontSize: 15,
    fontWeight: '700',
    marginBottom: 2,
  },
  handle: {
    color: '#71767b',
    fontSize: 15,
    marginBottom: 4,
  },
  text: {
    color: '#ffffff',
    fontSize: 15,
    lineHeight: 20,
  },
});
