import { useLayoutEffect, useRef } from 'react';
import type { ReactNode } from 'react';
import { View, Pressable, StyleSheet } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import type { ParamListBase } from '@react-navigation/native';
import type { DrawerNavigationProp } from '@react-navigation/drawer';
import { Chevron, Viewfinder } from './icons';
import { colors } from './theme';

export type HeaderActionHandlers = {
  onPrepend: () => void;
  onAppend: () => void;
  onScrollToRandom: () => void;
};

const HeaderButton = ({
  onPress,
  children,
}: {
  onPress: () => void;
  children: ReactNode;
}) => (
  <Pressable
    onPress={onPress}
    hitSlop={8}
    style={({ pressed }) => [styles.button, pressed && styles.pressed]}
  >
    {children}
  </Pressable>
);

/*
 * Renders the per-screen list controls (prepend / append / scroll-to-random) as
 * tinted nav-bar trailing buttons — the standard iOS location for screen
 * actions, so they never float over content. Handlers are read through a ref so
 * the buttons always call the latest closures without re-setting nav options.
 */
export function useHeaderActions(handlers: HeaderActionHandlers) {
  const navigation = useNavigation<DrawerNavigationProp<ParamListBase>>();
  const ref = useRef(handlers);
  ref.current = handlers;

  useLayoutEffect(() => {
    navigation.setOptions({
      headerRight: () => (
        <View style={styles.container}>
          <HeaderButton onPress={() => ref.current.onPrepend()}>
            <Chevron
              direction="up"
              color={colors.accent}
              size={22}
              weight={2.25}
            />
          </HeaderButton>
          <HeaderButton onPress={() => ref.current.onAppend()}>
            <Chevron
              direction="down"
              color={colors.accent}
              size={22}
              weight={2.25}
            />
          </HeaderButton>
          <HeaderButton onPress={() => ref.current.onScrollToRandom()}>
            <Viewfinder color={colors.accent} size={22} weight={2} />
          </HeaderButton>
        </View>
      ),
    });
  }, [navigation]);
}

const styles = StyleSheet.create({
  container: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    paddingRight: 12,
  },
  button: {
    width: 34,
    height: 34,
    alignItems: 'center',
    justifyContent: 'center',
  },
  pressed: {
    opacity: 0.35,
  },
});
