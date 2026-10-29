/*
 * The view that shows an item. Subclasses build their content into view. The list sets the
 * view's size along the cross axis and measures its size along the scroll axis once, then places
 * it at an exact frame. An item whose content changed size needs reloadItems.
 */
export class ShadowListKitListCell {
  static readonly NO_INDEX = -1;

  readonly view: HTMLDivElement;

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
   * The frame the list placed the view at, which lets a cell that did not move skip its
   * style writes.
   */
  placedX = Number.NaN;
  placedY = Number.NaN;
  placedWidth = Number.NaN;
  placedHeight = Number.NaN;

  /*
   * The list position and size the list last wrote to the view's aria attributes, which lets a
   * cell that kept both skip reading them.
   */
  accessiblePosition = -1;
  accessibleSetSize = -1;

  constructor(readonly reuseIdentifier: string | null = null) {
    this.view = document.createElement('div');
    const style = this.view.style;
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
