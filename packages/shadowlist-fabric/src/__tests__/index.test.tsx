import { describe, expect, it } from '@jest/globals';
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
