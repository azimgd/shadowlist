import type { ShadowListKitListCell } from './ShadowListKitListCell.js';
import type { ShadowListKitListView } from './ShadowListKitListView.js';

/*
 * Sticky rows stick through CSS. The cell of a mounted sticky row sits in a section element
 * that runs from the row to the next sticky row, and the cell is position sticky inside it. The
 * browser pins it at the viewport start and the section's end pushes it away, on the thread that
 * scrolls. No scroll frame moves it from here. The sticky frames are copied from the core when
 * the geometry changes. A pinned header view or footer view sticks the same way inside a span
 * the core gives, see pinTemplate.
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

  /*
   * The span element of each pinned header view and footer view.
   */
  private readonly templateSpans = new Map<HTMLElement, HTMLDivElement>();

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
      cell.view.style.width = `${w}px`;
      cell.view.style.height = `${h}px`;
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
    cell.view.replaceWith(section);
    section.append(cell.view);
    const style = cell.view.style;
    style.position = 'sticky';
    style.transform = '';
    style.pointerEvents = 'auto';
    this.sections.set(cell, section);
    return section;
  }

  /*
   * Pin a header view or a footer view: it sits in a span element that runs from start to end
   * along the axis, the core's stickySpans, position sticky at the viewport start, or at the
   * viewport end with atEnd. The span's edges push it away. A footer rests at the end of its
   * span. The span sits under the sections of the sticky rows, which come after it.
   */
  pinTemplate(
    view: HTMLElement,
    atEnd: boolean,
    start: number,
    end: number,
    cross: number
  ): void {
    let span = this.templateSpans.get(view);
    if (!span) {
      span = makeSection();
      span.style.display = 'flex';
      view.replaceWith(span);
      span.append(view);
      const style = view.style;
      style.position = 'sticky';
      style.transform = '';
      style.flex = 'none';
      style.pointerEvents = 'auto';
      this.templateSpans.set(view, span);
    }
    const horizontal = this.list.horizontal;
    const style = span.style;
    style.flexDirection = horizontal ? 'row' : 'column';
    style.justifyContent = atEnd ? 'flex-end' : 'flex-start';
    style.left = horizontal ? `${start}px` : '0';
    style.top = horizontal ? '0' : `${start}px`;
    style.width = `${horizontal ? end - start : cross}px`;
    style.height = `${horizontal ? cross : end - start}px`;
    const edges = view.style;
    edges.left = !atEnd && horizontal ? '0' : '';
    edges.top = !atEnd && !horizontal ? '0' : '';
    edges.right = atEnd && horizontal ? '0' : '';
    edges.bottom = atEnd && !horizontal ? '0' : '';
  }

  /*
   * Take a header view or a footer view out of its span, back where the span stood.
   */
  unpinTemplate(view: HTMLElement): void {
    const span = this.templateSpans.get(view);
    if (!span) return;
    this.templateSpans.delete(view);
    span.replaceWith(view);
    const style = view.style;
    style.position = 'absolute';
    style.left = '0';
    style.top = '0';
    style.right = '';
    style.bottom = '';
    style.flex = '';
    style.pointerEvents = '';
  }

  /*
   * Take a cell out of its section, back among the rows. Its next placement writes every style
   * again.
   */
  release(cell: ShadowListKitListCell): void {
    const section = this.sections.get(cell);
    if (!section) return;
    this.sections.delete(cell);
    section.replaceWith(cell.view);
    const style = cell.view.style;
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
