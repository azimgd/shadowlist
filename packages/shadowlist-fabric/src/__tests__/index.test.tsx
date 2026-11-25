import { afterEach, describe, expect, it, jest } from '@jest/globals';
import TestRenderer, { act } from 'react-test-renderer';
import { useAnchorState } from '../virtualizer/useAnchorState';
import { EMPTY_SOURCE } from '../virtualizer/rowSource';
import * as shadowlist from '../index';

describe('package root', () => {
  it('exports the list components and native views', () => {
    expect(shadowlist.ShadowList).toBeDefined();
    expect(shadowlist.SectionList).toBeDefined();
    expect(shadowlist.TreeList).toBeDefined();
    expect(shadowlist.DraggableList).toBeDefined();
    expect(shadowlist.KeyboardView).toBeDefined();
    expect(typeof shadowlist.useKeyboardAnimation).toBe('function');
    expect(shadowlist.ShadowListView).toBeDefined();
    expect(shadowlist.ShadowListCellView).toBeDefined();
    expect(shadowlist.ShadowListTemplateView).toBeDefined();
  });

  it('exports the native view commands', () => {
    expect(typeof shadowlist.Commands.scrollToRow).toBe('function');
  });
});

function renderAnchor(nativeView = true) {
  const viewRef: Parameters<typeof useAnchorState>[0]['viewRef'] = {
    current: nativeView
      ? ({} as NonNullable<
          Parameters<typeof useAnchorState>[0]['viewRef']['current']
        >)
      : null,
  };
  let anchor!: ReturnType<typeof useAnchorState>;
  function Probe() {
    anchor = useAnchorState({
      viewRef,
      source: EMPTY_SOURCE,
      seedAroundIndex: () => {},
    });
    return null;
  }
  let renderer!: TestRenderer.ReactTestRenderer;
  act(() => {
    renderer = TestRenderer.create(<Probe />);
  });
  return { anchor, renderer };
}

describe('anchor requests', () => {
  afterEach(() => {
    jest.restoreAllMocks();
  });

  it('sends every request while one answer drains all waiters', async () => {
    const request = jest
      .spyOn(shadowlist.Commands, 'requestAnchorState')
      .mockImplementation(() => {});
    const { anchor, renderer } = renderAnchor();
    const first = anchor.requestAnchorState();
    const second = anchor.requestAnchorState();
    expect(request).toHaveBeenCalledTimes(2);
    anchor.handleAnchorState({
      nativeEvent: { found: true, key: 'row', offset: 12 },
    });
    await expect(first).resolves.toEqual({ key: 'row', offset: 12 });
    await expect(second).resolves.toEqual({ key: 'row', offset: 12 });
    const third = anchor.requestAnchorState();
    expect(request).toHaveBeenCalledTimes(3);
    anchor.handleAnchorState({
      nativeEvent: { found: false, key: '', offset: 0 },
    });
    await expect(third).resolves.toBeNull();
    act(() => renderer.unmount());
  });

  it('answers every waiter with null on unmount', async () => {
    jest
      .spyOn(shadowlist.Commands, 'requestAnchorState')
      .mockImplementation(() => {});
    const { anchor, renderer } = renderAnchor();
    const first = anchor.requestAnchorState();
    const second = anchor.requestAnchorState();
    act(() => renderer.unmount());
    await expect(first).resolves.toBeNull();
    await expect(second).resolves.toBeNull();
    anchor.handleAnchorState({
      nativeEvent: { found: true, key: 'late', offset: 0 },
    });
  });

  it('answers null without sending when no native view exists', async () => {
    const request = jest
      .spyOn(shadowlist.Commands, 'requestAnchorState')
      .mockImplementation(() => {});
    const { anchor, renderer } = renderAnchor(false);
    await expect(anchor.requestAnchorState()).resolves.toBeNull();
    expect(request).not.toHaveBeenCalled();
    act(() => renderer.unmount());
  });
});
