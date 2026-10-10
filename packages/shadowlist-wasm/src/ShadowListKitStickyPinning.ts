import type { ShadowListKitListCell } from './ShadowListKitListCell.js';
import type { ShadowListKitListView } from './ShadowListKitListView.js';

/*
 * Sticky rows stick through CSS. The cell of a mounted sticky row sits in a section element
 * that runs from the row to the next sticky row, and the cell is position sticky inside it. The
 * browser pins it at the viewport start and the section's end pushes it away, on the thread that
 * scrolls. No scroll frame moves it from here. The sticky frames are copied from the core when
 * the geometry changes.
 */
export class ShadowListKitStickyPinning {
  /*
   * The sticky rows, sorted, each once.
   */
  stickyRows: number[] = [];

  /*
   * Leading edge and extent of every sticky row, copied from the core when the geometry
   * changes.
   */
  private stickyFrames = new Float64Array(0);
  private stickyGeometry = -1;

  /*
   * The section element of each cell placed as a sticky row, and sections free for reuse.
   */
  private readonly sections = new Map<ShadowListKitListCell, HTMLDivElement>();
  private readonly spareSections: HTMLDivElement[] = [];

  constructor(private readonly list: ShadowListKitListView) {}

  /*
   * Read the sticky rows again. Returns whether they changed.
   */
  updateStickyRows(stickyIndices: ReadonlyArray<number>): boolean {
    const sorted = Array.from(
      new Set(stickyIndices.filter((index) => index >= 0))
    ).sort((a, b) => a - b);
    if (
      sorted.length === this.stickyRows.length &&
      sorted.every((row, position) => row === this.stickyRows[position])
    ) {
      return false;
    }
    this.stickyRows = sorted;
    this.list.liveCore?.setStickyIndices(sorted);
    this.stickyGeometry = -1;
    return true;
  }

  /*
   * Copy the frames again on the next use, like after a new core.
   */
  invalidateFrames(): void {
    this.stickyGeometry = -1;
  }

  private refreshStickyFrames(): void {
    if (this.stickyGeometry === this.list.frameGeometry) return;
    if (this.stickyFrames.length < this.stickyRows.length * 2) {
      this.stickyFrames = new Float64Array(this.stickyRows.length * 2);
    }
    if (this.list.core.copyStickyFrames(this.stickyFrames)) {
      this.stickyGeometry = this.list.frameGeometry;
    }
  }

  /*
   * Position in stickyRows of the last sticky row starting at or above the offset, or -1. Rows
   * the core has not placed yet start at infinity and sort last. The core test
   * list_driver_sticky_frames_pin_the_same_header_as_the_driver holds the rule this matches.
   */
  private activeStickyPosition(offset: number): number {
    if (this.stickyRows.length === 0) return -1;
    this.refreshStickyFrames();
    let found = -1;
    let low = 0;
    let high = this.stickyRows.length;
    while (low < high) {
      const mid = Math.floor((low + high) / 2);
      if (this.stickyFrames[mid * 2]! <= offset) {
        found = mid;
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    return found;
  }

  /*
   * The sticky row pinned at this offset. The list keeps it mounted wherever its own row is.
   */
  activeStickyIndex(offset: number): number {
    const position = this.activeStickyPosition(offset);
    return position >= 0 ? this.stickyRows[position]! : -1;
  }

  /*
   * Position of a row in stickyRows, or -1 when it does not stick.
   */
  private stickyPosition(row: number): number {
    let low = 0;
    let high = this.stickyRows.length;
    while (low < high) {
      const mid = Math.floor((low + high) / 2);
      const value = this.stickyRows[mid]!;
      if (value === row) return mid;
      if (value < row) low = mid + 1;
      else high = mid;
    }
    return -1;
  }

  /*
   * Place a sticky row's cell in its section at the row's frame, x y width height. Returns
   * false for a row that does not stick, which leaves any section it was in.
   */
  placeSticky(
    cell: ShadowListKitListCell,
    row: number,
    frame: Float64Array
  ): boolean {
    const position = this.stickyPosition(row);
    if (position < 0) {
      this.release(cell);
      return false;
    }
    this.refreshStickyFrames();
    const horizontal = this.list.horizontal;
    const [x, y, w, h] = [frame[0]!, frame[1]!, frame[2]!, frame[3]!];
    const leading = horizontal ? x : y;
    // The section ends where the next sticky row starts, or where the rows end.
    const next = this.stickyFrames[(position + 1) * 2];
    const end =
      position + 1 < this.stickyRows.length && Number.isFinite(next)
        ? next!
        : this.list.rowsEnd;
    const span = Math.max(end - leading, horizontal ? w : h);
    // Sticky works from layout positions and ignores transforms. The section moves with left and top.
    const section = this.sectionOf(cell);
    const style = section.style;
    style.left = `${x}px`;
    style.top = `${y}px`;
    style.width = `${horizontal ? span : w}px`;
    style.height = `${horizontal ? h : span}px`;
    if (cell.placedWidth !== w || cell.placedHeight !== h) {
      cell.element.style.width = `${w}px`;
      cell.element.style.height = `${h}px`;
    }
    cell.placedX = x;
    cell.placedY = y;
    cell.placedWidth = w;
    cell.placedHeight = h;
    return true;
  }

  /*
   * The cell's section, moving the cell into a new one where it stood.
   */
  private sectionOf(cell: ShadowListKitListCell): HTMLDivElement {
    const known = this.sections.get(cell);
    if (known) return known;
    const section = this.spareSections.pop() ?? makeSection();
    cell.element.replaceWith(section);
    section.append(cell.element);
    const style = cell.element.style;
    style.position = 'sticky';
    style.transform = '';
    style.pointerEvents = 'auto';
    this.sections.set(cell, section);
    return section;
  }

  /*
   * Take a cell out of its section, back among the rows. Its next placement writes every style
   * again.
   */
  release(cell: ShadowListKitListCell): void {
    const section = this.sections.get(cell);
    if (!section) return;
    this.sections.delete(cell);
    section.replaceWith(cell.element);
    const style = cell.element.style;
    style.position = 'absolute';
    style.pointerEvents = '';
    cell.resetPlacement();
    this.spareSections.push(section);
  }
}

/*
 * A section element: above the rows, and letting touches through to the rows under it.
 */
function makeSection(): HTMLDivElement {
  const section = document.createElement('div');
  const style = section.style;
  style.position = 'absolute';
  style.zIndex = '1';
  style.pointerEvents = 'none';
  return section;
}
