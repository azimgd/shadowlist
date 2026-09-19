import { useLayoutEffect, useRef } from 'react';
import type { ReactNode, RefObject } from 'react';
import { View, Pressable, StyleSheet } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import type { ParamListBase } from '@react-navigation/native';
import type { DrawerNavigationProp } from '@react-navigation/drawer';
import { ChevronIcon, useTheme } from 'shadowlist-utils/native';
import { ViewfinderIcon } from './icons';

export interface HeaderActionHandlers {
  onPrepend: () => void;
  onAppend: () => void;
  onScrollToRandom: () => void;
}

interface HeaderButtonProps {
  onPress: () => void;
  // Icon-only buttons need a spoken name.
  label: string;
  children: ReactNode;
}

const HeaderButton = ({ onPress, label, children }: HeaderButtonProps) => (
  <Pressable
    onPress={onPress}
    accessibilityRole="button"
    accessibilityLabel={label}
    hitSlop={8}
    style={({ pressed }) => [styles.button, pressed && styles.pressed]}
  >
    {children}
  </Pressable>
);

const HeaderActionsBar = ({
  handlers,
}: {
  handlers: RefObject<HeaderActionHandlers>;
}) => {
  const { colors } = useTheme();
  return (
    <View style={styles.container}>
      <HeaderButton
        label="Load earlier"
        onPress={() => handlers.current.onPrepend()}
      >
        <ChevronIcon
          direction="up"
          color={colors.accent}
          size={22}
          strokeWidth={2.25}
        />
      </HeaderButton>
      <HeaderButton
        label="Add new items"
        onPress={() => handlers.current.onAppend()}
      >
        <ChevronIcon
          direction="down"
          color={colors.accent}
          size={22}
          strokeWidth={2.25}
        />
      </HeaderButton>
      <HeaderButton
        label="Jump to a random item"
        onPress={() => handlers.current.onScrollToRandom()}
      >
        <ViewfinderIcon color={colors.accent} size={22} strokeWidth={2} />
      </HeaderButton>
    </View>
  );
};

/*
 * Renders the per-screen list controls (prepend / append / scroll-to-random) as
 * tinted nav-bar trailing buttons, the standard iOS location for screen actions,
 * so they never float over content. Handlers are read through a ref so the
 * buttons always call the latest closures without re-setting nav options.
 */
export function useHeaderActions(handlers: HeaderActionHandlers) {
  const navigation = useNavigation<DrawerNavigationProp<ParamListBase>>();
  const ref = useRef(handlers);
  ref.current = handlers;

  useLayoutEffect(() => {
    navigation.setOptions({
      headerRight: () => <HeaderActionsBar handlers={ref} />,
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
