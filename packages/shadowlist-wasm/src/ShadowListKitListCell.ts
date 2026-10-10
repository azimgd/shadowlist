/*
 * A row view. Subclasses build their content into element. The list sets the element's size
 * along the cross axis and measures its size along the scroll axis once, then places it at an
 * exact frame. A row whose content changed size needs reloadItems.
 */
export class ShadowListKitListCell {
  static readonly NO_INDEX = -1;

  readonly element: HTMLDivElement;

  /*
   * The item the cell shows, or NO_INDEX while it waits in the reuse pool. Set by the list.
   */
  index = ShadowListKitListCell.NO_INDEX;

  /*
   * The mount pass that last wanted this cell. Cells left behind go back to the pool. Set by
   * the list.
   */
  mountGeneration = 0;

  /*
   * The frame the list placed the element at, which lets a cell that did not move skip its
   * style writes.
   */
  placedX = Number.NaN;
  placedY = Number.NaN;
  placedWidth = Number.NaN;
  placedHeight = Number.NaN;

  constructor(readonly reuseIdentifier: string | null = null) {
    this.element = document.createElement('div');
    const style = this.element.style;
    style.position = 'absolute';
    style.left = '0';
    style.top = '0';
    style.boxSizing = 'border-box';
    style.visibility = 'hidden';
  }

  /*
   * Called before the cell is handed out again by dequeueReusableCell.
   */
  prepareForReuse(): void {}

  /*
   * Forget the placed frame. The next placement writes every style again.
   */
  resetPlacement(): void {
    this.placedX = Number.NaN;
    this.placedY = Number.NaN;
    this.placedWidth = Number.NaN;
    this.placedHeight = Number.NaN;
  }
}
