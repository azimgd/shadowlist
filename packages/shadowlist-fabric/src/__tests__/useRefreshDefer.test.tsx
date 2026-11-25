import { afterEach, describe, expect, it, jest } from '@jest/globals';
import TestRenderer, { act } from 'react-test-renderer';
import { Commands } from '../ShadowListViewNativeComponent';
import { useRefreshDefer } from '../virtualizer/useRefreshDefer';
import * as helpers from '../virtualizer/helpers';

interface Rows {
  count: number;
}

interface ProbeProps {
  data: Rows;
  refreshing: boolean;
  onRefresh: (() => void) | undefined;
  inverted?: boolean;
  horizontal?: boolean;
}

/*
 * Renders the hook and keeps the data it shows.
 */
function renderDefer(nativeView = false) {
  const viewRef: Parameters<typeof useRefreshDefer>[0]['viewRef'] = {
    current: nativeView
      ? ({} as NonNullable<
          Parameters<typeof useRefreshDefer>[0]['viewRef']['current']
        >)
      : null,
  };
  let settle = () => {};
  const shown: Rows[] = [];
  function Probe({
    data,
    refreshing,
    onRefresh,
    inverted = false,
    horizontal = false,
  }: ProbeProps) {
    const result = useRefreshDefer({
      data,
      refreshing,
      onRefresh,
      inverted,
      horizontal,
      viewRef,
    });
    shown.push(result.data);
    settle = result.handleRefreshSettle;
    return null;
  }
  return { Probe, shown, settle: () => settle() };
}

describe('useRefreshDefer', () => {
  afterEach(() => {
    jest.restoreAllMocks();
  });
  it('holds data during a refresh and lets it go once onRefresh is removed', () => {
    const { Probe, shown } = renderDefer();
    const onRefresh = () => {};
    const first = { count: 1 };
    const second = { count: 2 };
    let renderer!: TestRenderer.ReactTestRenderer;
    act(() => {
      renderer = TestRenderer.create(
        <Probe data={first} refreshing={false} onRefresh={onRefresh} />
      );
    });
    act(() => {
      renderer.update(
        <Probe data={first} refreshing={true} onRefresh={onRefresh} />
      );
    });
    act(() => {
      renderer.update(
        <Probe data={second} refreshing={true} onRefresh={onRefresh} />
      );
    });
    expect(shown[shown.length - 1]).toBe(first);
    act(() => {
      renderer.update(
        <Probe data={second} refreshing={true} onRefresh={undefined} />
      );
    });
    expect(shown[shown.length - 1]).toBe(second);
  });

  it('shows the held data once the refresh ends on a list without a native view', () => {
    const { Probe, shown } = renderDefer();
    const onRefresh = () => {};
    const first = { count: 1 };
    const second = { count: 2 };
    let renderer!: TestRenderer.ReactTestRenderer;
    act(() => {
      renderer = TestRenderer.create(
        <Probe data={first} refreshing={false} onRefresh={onRefresh} />
      );
    });
    act(() => {
      renderer.update(
        <Probe data={first} refreshing={true} onRefresh={onRefresh} />
      );
    });
    act(() => {
      renderer.update(
        <Probe data={second} refreshing={true} onRefresh={onRefresh} />
      );
    });
    expect(shown[shown.length - 1]).toBe(first);
    act(() => {
      renderer.update(
        <Probe data={second} refreshing={false} onRefresh={onRefresh} />
      );
    });
    expect(shown[shown.length - 1]).toBe(second);
  });

  it('requests once, holds edits through settle, and releases the newest prop', () => {
    const request = jest
      .spyOn(Commands, 'awaitRefreshSettle')
      .mockImplementation(() => {});
    const { Probe, shown, settle } = renderDefer(true);
    const onRefresh = () => {};
    const first = { count: 2 };
    const edit = { count: 2 };
    const newest = { count: 1 };
    let renderer!: TestRenderer.ReactTestRenderer;
    act(() => {
      renderer = TestRenderer.create(
        <Probe data={first} refreshing={false} onRefresh={onRefresh} />
      );
    });
    act(() => {
      renderer.update(
        <Probe data={edit} refreshing={true} onRefresh={onRefresh} />
      );
    });
    act(() => {
      renderer.update(
        <Probe data={edit} refreshing={false} onRefresh={onRefresh} />
      );
    });
    act(() => {
      renderer.update(
        <Probe data={newest} refreshing={false} onRefresh={onRefresh} />
      );
    });
    expect(request).toHaveBeenCalledTimes(1);
    expect(shown[shown.length - 1]).toBe(first);
    act(settle);
    expect(shown[shown.length - 1]).toBe(newest);
    const renders = shown.length;
    act(settle);
    expect(shown).toHaveLength(renders);
    act(() => renderer.unmount());
  });

  it('keeps the displayed rows when refresh restarts before settle', () => {
    const request = jest
      .spyOn(Commands, 'awaitRefreshSettle')
      .mockImplementation(() => {});
    const { Probe, shown, settle } = renderDefer(true);
    const onRefresh = () => {};
    const first = { count: 1 };
    const second = { count: 2 };
    let renderer!: TestRenderer.ReactTestRenderer;
    act(() => {
      renderer = TestRenderer.create(
        <Probe data={first} refreshing={false} onRefresh={onRefresh} />
      );
    });
    for (const refreshing of [true, false, true, false]) {
      act(() => {
        renderer.update(
          <Probe data={second} refreshing={refreshing} onRefresh={onRefresh} />
        );
      });
      expect(shown[shown.length - 1]).toBe(first);
    }
    expect(request).toHaveBeenCalledTimes(2);
    act(settle);
    expect(shown[shown.length - 1]).toBe(second);
    act(() => renderer.unmount());
  });

  it('does not freeze an initially refreshing list but still requests settle', () => {
    const request = jest
      .spyOn(Commands, 'awaitRefreshSettle')
      .mockImplementation(() => {});
    const { Probe, shown } = renderDefer(true);
    const onRefresh = () => {};
    const first = { count: 1 };
    const second = { count: 2 };
    let renderer!: TestRenderer.ReactTestRenderer;
    act(() => {
      renderer = TestRenderer.create(
        <Probe data={first} refreshing={true} onRefresh={onRefresh} />
      );
    });
    act(() => {
      renderer.update(
        <Probe data={second} refreshing={true} onRefresh={onRefresh} />
      );
    });
    expect(shown[shown.length - 1]).toBe(second);
    act(() => {
      renderer.update(
        <Probe data={second} refreshing={false} onRefresh={onRefresh} />
      );
    });
    expect(request).toHaveBeenCalledTimes(1);
    act(() => renderer.unmount());
  });

  it.each(['horizontal', 'inverted'] as const)(
    'releases held rows when %s is enabled',
    (axis) => {
      const { Probe, shown } = renderDefer(true);
      const onRefresh = () => {};
      const first = { count: 1 };
      const second = { count: 2 };
      let renderer!: TestRenderer.ReactTestRenderer;
      act(() => {
        renderer = TestRenderer.create(
          <Probe data={first} refreshing={false} onRefresh={onRefresh} />
        );
      });
      act(() => {
        renderer.update(
          <Probe data={second} refreshing={true} onRefresh={onRefresh} />
        );
      });
      expect(shown[shown.length - 1]).toBe(first);
      act(() => {
        renderer.update(
          <Probe
            data={second}
            refreshing={true}
            onRefresh={onRefresh}
            {...{ [axis]: true }}
          />
        );
      });
      expect(shown[shown.length - 1]).toBe(second);
      act(() => renderer.unmount());
    }
  );

  it('does not render again on settle when no data was held', () => {
    jest.spyOn(Commands, 'awaitRefreshSettle').mockImplementation(() => {});
    jest.spyOn(helpers, 'slTraceEnabled').mockReturnValue(true);
    const trace = jest.spyOn(helpers, 'slTrace').mockImplementation(() => {});
    const { Probe, shown, settle } = renderDefer(true);
    const onRefresh = () => {};
    const data = { count: 1 };
    let renderer!: TestRenderer.ReactTestRenderer;
    act(() => {
      renderer = TestRenderer.create(
        <Probe data={data} refreshing={false} onRefresh={onRefresh} />
      );
    });
    for (const refreshing of [true, false]) {
      act(() => {
        renderer.update(
          <Probe data={data} refreshing={refreshing} onRefresh={onRefresh} />
        );
      });
    }
    const renders = shown.length;
    act(settle);
    expect(shown).toHaveLength(renders);
    expect(trace.mock.calls).toEqual([['refresh settle held=0']]);
    act(() => renderer.unmount());
  });

  it('traces each new held value once and remembers holding when data reverts', () => {
    jest.spyOn(Commands, 'awaitRefreshSettle').mockImplementation(() => {});
    jest.spyOn(helpers, 'slTraceEnabled').mockReturnValue(true);
    const trace = jest.spyOn(helpers, 'slTrace').mockImplementation(() => {});
    const { Probe, shown, settle } = renderDefer(true);
    const onRefresh = () => {};
    const first = { count: 1 };
    const second = { count: 2 };
    const third = { count: 3 };
    let renderer!: TestRenderer.ReactTestRenderer;
    act(() => {
      renderer = TestRenderer.create(
        <Probe data={first} refreshing={false} onRefresh={onRefresh} />
      );
    });
    for (const data of [second, second, third, first, third, first]) {
      act(() => {
        renderer.update(
          <Probe data={data} refreshing={true} onRefresh={onRefresh} />
        );
      });
      expect(shown[shown.length - 1]).toBe(first);
    }
    act(() => {
      renderer.update(
        <Probe data={first} refreshing={false} onRefresh={onRefresh} />
      );
    });
    const renders = shown.length;
    act(settle);
    expect(shown).toHaveLength(renders + 1);
    expect(trace.mock.calls).toEqual([
      ['refresh hold n=2'],
      ['refresh hold n=3'],
      ['refresh settle held=1'],
    ]);
    expect(shown[shown.length - 1]).toBe(first);
    act(() => renderer.unmount());
  });
});
