import { describe, expect, it } from '@jest/globals';
import { ExtraWindow, type ExtraItem } from '../native/extras';

function setup(padding = 0, minPad = 1) {
  const patches: [number, ExtraItem][] = [];
  const window = new ExtraWindow(
    { update: (index, patch) => patches.push([index, patch]), padding, minPad },
    { start: 0, end: 1 }
  );
  return { window, patches };
}

describe('ExtraWindow', () => {
  it('seeds static extras everywhere and materialized ones around the window', () => {
    const { window, patches } = setup();
    const seeded = window.reset(
      10,
      [
        { index: 1, item: { t: 'group', g: 'A' } },
        { index: 8, item: { t: 'group', g: 'B' } },
      ],
      (index) => (index % 2 === 0 ? { sel: index } : null)
    );
    // Visible 0 to 1, padded by one row each side, gives 0 to 2.
    expect(window.materializedIndices()).toEqual([0, 1, 2]);
    expect(seeded).toEqual([
      { index: 8, item: { t: 'group', g: 'B' } },
      { index: 0, item: { sel: 0 } },
      { index: 1, item: { t: 'group', g: 'A' } },
      { index: 2, item: { sel: 2 } },
    ]);
    expect(patches).toEqual([]);
  });

  it('materializes rows entering the window and releases rows far away', () => {
    const { window, patches } = setup();
    window.reset(10, [{ index: 7, item: { g: 'B' } }], (index) => ({
      n: index,
    }));
    window.setWindow({ start: 6, end: 7 });
    // Builds 5 to 8 and keeps 4 to 9. Rows 0 to 2 go back to their static item, which is empty.
    expect(window.materializedIndices()).toEqual([5, 6, 7, 8]);
    expect(patches).toEqual([
      [0, { n: null }],
      [1, { n: null }],
      [2, { n: null }],
      [5, { n: 5 }],
      [6, { n: 6 }],
      [7, { n: 7 }],
      [8, { n: 8 }],
    ]);
  });

  it('patches only changed fields on refresh, arrays by content', () => {
    const { window, patches } = setup();
    let focus = 0;
    const getExtra = (index: number) => ({
      texts: ['a', String(index)],
      ...(index === focus ? { focus: 1 } : null),
    });
    window.reset(5, [], getExtra);
    focus = 1;
    window.refresh();
    expect(patches).toEqual([
      [0, { focus: null }],
      [1, { focus: 1 }],
    ]);
  });

  it('re-derives on a new getExtra and drops everything without one', () => {
    const { window, patches } = setup();
    window.reset(3, [{ index: 0, item: { g: 'A' } }], () => ({ v: 1 }));
    window.setGetExtra(() => ({ v: 2 }));
    expect(patches).toEqual([
      [0, { v: 2 }],
      [1, { v: 2 }],
      [2, { v: 2 }],
    ]);
    patches.length = 0;
    window.setGetExtra(null);
    // Row 0 falls back to its static extra.
    expect(patches).toEqual([
      [0, { v: null }],
      [1, { v: null }],
      [2, { v: null }],
    ]);
    expect(window.materializedIndices()).toEqual([]);
  });
});
