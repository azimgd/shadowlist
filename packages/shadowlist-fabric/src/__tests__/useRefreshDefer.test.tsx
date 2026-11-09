import { describe, expect, it } from '@jest/globals';
import TestRenderer, { act } from 'react-test-renderer';
import { useRefreshDefer } from '../virtualizer/useRefreshDefer';

interface Rows {
  count: number;
}

interface ProbeProps {
  data: Rows;
  refreshing: boolean;
  onRefresh: (() => void) | undefined;
}

/*
 * Renders the hook and keeps the data it shows.
 */
function renderDefer() {
  const shown: Rows[] = [];
  function Probe({ data, refreshing, onRefresh }: ProbeProps) {
    const result = useRefreshDefer({
      data,
      refreshing,
      onRefresh,
      inverted: false,
      horizontal: false,
    });
    shown.push(result.data);
    return null;
  }
  return { Probe, shown };
}

describe('useRefreshDefer', () => {
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
});
