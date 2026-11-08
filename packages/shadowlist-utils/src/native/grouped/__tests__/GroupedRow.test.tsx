/*
 * Grouped.Row imports react-native and shadowlist, which do not load under Node. The mocks draw
 * their components as named host elements, so the tests read the props the row gives them.
 */
jest.mock(
  'react-native',
  () => ({
    Platform: {
      OS: 'ios',
      select: (options: { ios?: unknown }) => options.ios,
    },
    Appearance: {
      getColorScheme: () => 'light',
      addChangeListener: () => ({ remove: () => {} }),
    },
    PixelRatio: { getFontScale: () => 1 },
    PlatformColor: (...names: string[]) => names[0],
    StyleSheet: {
      create: <T,>(styles: T) => styles,
      hairlineWidth: 1,
    },
    useWindowDimensions: () => ({ width: 400, height: 1000, fontScale: 1 }),
    Pressable: 'Pressable',
    View: 'View',
    Text: 'Text',
  }),
  { virtual: true }
);
jest.mock('shadowlist', () => ({ ItemActionsView: 'ItemActionsView' }), {
  virtual: true,
});
jest.mock('../../icons', () => ({ ChevronIcon: 'ChevronIcon' }));
jest.mock('../../hooks/useLargeText', () => ({ useLargeText: () => false }));

import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import { GroupedRow } from '../GroupedRow';

Object.assign(globalThis, {
  IS_REACT_ACT_ENVIRONMENT: true,
  IS_REACT_NATIVE_TEST_ENVIRONMENT: true,
});

const MENU = { actions: [{ title: 'Rename', onPress: () => {} }] };

function mount(onPress: () => void, withMenu = true): ReactTestRenderer {
  let renderer: ReactTestRenderer | undefined;
  act(() => {
    renderer = TestRenderer.create(
      <GroupedRow
        title="Clients"
        onPress={onPress}
        contextMenu={withMenu ? MENU : undefined}
      />
    );
  });
  return renderer!;
}

function pressable(renderer: ReactTestRenderer) {
  return renderer.root.findByType('Pressable' as never);
}

function menuHost(renderer: ReactTestRenderer) {
  return renderer.root.findByType('ItemActionsView' as never);
}

describe('Grouped.Row with a menu', () => {
  it('opens on a tap', () => {
    const onPress = jest.fn();
    const row = pressable(mount(onPress));
    row.props.onPressIn();
    row.props.onPress();
    expect(onPress).toHaveBeenCalledTimes(1);
  });

  it('does not open when the finger lifts after the menu said it is coming', () => {
    const onPress = jest.fn();
    const renderer = mount(onPress);
    pressable(renderer).props.onPressIn();
    menuHost(renderer).props.onContextMenuWillShow();
    pressable(renderer).props.onPress();
    expect(onPress).not.toHaveBeenCalled();
  });

  it('opens on the next tap after a hold', () => {
    const onPress = jest.fn();
    const renderer = mount(onPress);
    pressable(renderer).props.onPressIn();
    menuHost(renderer).props.onContextMenuWillShow();
    pressable(renderer).props.onPress();
    pressable(renderer).props.onPressIn();
    pressable(renderer).props.onPress();
    expect(onPress).toHaveBeenCalledTimes(1);
  });
});

describe('Grouped.Row held, with a menu', () => {
  beforeEach(() => jest.useFakeTimers());
  afterEach(() => jest.useRealTimers());

  it('does not open when the menu says so just after the finger lifts', () => {
    const onPress = jest.fn();
    const renderer = mount(onPress);
    pressable(renderer).props.onPressIn();
    jest.advanceTimersByTime(400);
    pressable(renderer).props.onPress();
    menuHost(renderer).props.onContextMenuWillShow();
    jest.advanceTimersByTime(1000);
    expect(onPress).not.toHaveBeenCalled();
  });

  it('opens a beat later when no menu comes', () => {
    const onPress = jest.fn();
    const renderer = mount(onPress);
    pressable(renderer).props.onPressIn();
    jest.advanceTimersByTime(400);
    pressable(renderer).props.onPress();
    expect(onPress).not.toHaveBeenCalled();
    jest.advanceTimersByTime(1000);
    expect(onPress).toHaveBeenCalledTimes(1);
  });
});

describe('Grouped.Row without a menu', () => {
  it('hands the press over as it was given', () => {
    const onPress = jest.fn();
    const row = pressable(mount(onPress, false));
    expect(row.props.onPress).toBe(onPress);
  });
});
