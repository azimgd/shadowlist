import type { ShadowListKitListView } from './ShadowListKitListView.js';

/*
 * Duration of a snap after the list came to rest, in milliseconds.
 */
const SETTLE_DURATION_MS = 250;

/*
 * Duration of an animated scroll command, in milliseconds.
 */
const COMMAND_DURATION_MS = 300;

/*
 * Time without a scroll event after which the list counts as resting, for browsers without
 * the scrollend event, in milliseconds.
 */
const IDLE_DELAY_MS = 150;

/*
 * The browser scrolls the list. This follows what moves it to tell the core the scroll phase:
 * a finger or the scrollbar on it is dragging, momentum and animated commands are settling.
 * Animated commands and the snap after a scroll run here, one step per animation frame.
 */
export class ShadowListKitScrollGesture {
  isTracking = false;
  isAnimating = false;

  /*
   * The browser moved the list for the reader since the list last came to rest.
   */
  isScrolling = false;

  private idleTimer: ReturnType<typeof setTimeout> | null = null;

  /*
   * When the last wheel event came. A wheel or a trackpad sends scrollend after every step.
   */
  private previousWheel = 0;
  private animationFrame = 0;
  private animationFrom = 0;
  private animationTo = 0;

  /*
   * Where a command's animation aims, read again on every frame. Rows measured on the way move
   * the target, and the animation follows instead of landing with a jump.
   */
  private animationTarget: (() => number) | null = null;
  private animationStart = 0;
  private animationDuration = 0;

  constructor(private readonly list: ShadowListKitListView) {}

  attach(): void {
    const list = this.list;
    list.addEventListener('touchstart', this.onTouchStart, { passive: true });
    list.addEventListener('touchend', this.onTouchEnd, { passive: true });
    list.addEventListener('touchcancel', this.onTouchEnd, { passive: true });
    list.addEventListener('pointerdown', this.onPointerDown, { passive: true });
    list.addEventListener('pointerup', this.onPointerUp, { passive: true });
    list.addEventListener('pointercancel', this.onPointerUp, { passive: true });
    list.addEventListener('wheel', this.onWheel, { passive: true });
    list.addEventListener('scrollend', this.onScrollEnd, { passive: true });
  }

  detach(): void {
    const list = this.list;
    list.removeEventListener('touchstart', this.onTouchStart);
    list.removeEventListener('touchend', this.onTouchEnd);
    list.removeEventListener('touchcancel', this.onTouchEnd);
    list.removeEventListener('pointerdown', this.onPointerDown);
    list.removeEventListener('pointerup', this.onPointerUp);
    list.removeEventListener('pointercancel', this.onPointerUp);
    list.removeEventListener('wheel', this.onWheel);
    list.removeEventListener('scrollend', this.onScrollEnd);
    this.stop();
    this.clearIdleTimer();
    this.isTracking = false;
    this.isScrolling = false;
  }

  /*
   * The browser scrolled the list for the reader. The list rests once no scroll event follows
   * for a while or scrollend arrives.
   */
  userScrolled(): void {
    this.isScrolling = true;
    this.clearIdleTimer();
    this.idleTimer = setTimeout(this.onIdle, IDLE_DELAY_MS);
  }

  private onTouchStart = (): void => {
    this.beginGesture();
    this.isTracking = true;
  };

  private onTouchEnd = (): void => {
    this.isTracking = false;
    if (!this.isScrolling) this.settle();
  };

  /*
   * A mouse press on the list itself, not on a row, is on its scrollbar.
   */
  private onPointerDown = (event: PointerEvent): void => {
    if (event.pointerType !== 'mouse' || event.target !== this.list) return;
    this.beginGesture();
    this.isTracking = true;
  };

  private onPointerUp = (event: PointerEvent): void => {
    if (event.pointerType !== 'mouse' || !this.isTracking) return;
    this.isTracking = false;
    if (!this.isScrolling) this.settle();
  };

  private onWheel = (): void => {
    this.previousWheel = performance.now();
    this.beginGesture();
  };

  /*
   * A scrollend right after a wheel step is not the end. The idle timer ends the scroll once
   * the wheel stopped, which keeps a snap from pulling against the wheel.
   */
  private onScrollEnd = (): void => {
    if (this.isAnimating) return;
    if (performance.now() - this.previousWheel < IDLE_DELAY_MS) return;
    /*
     * Rest once this frame is done. A page that writes the offset on every frame gets the
     * scrollend of one write in the frame of the next, and a snap started now would move the
     * rows before that frame paints. A write that follows stops the snap before its first step.
     */
    this.clearIdleTimer();
    this.idleTimer = setTimeout(this.onIdle, 0);
  };

  private onIdle = (): void => {
    this.clearIdleTimer();
    if (this.isTracking || this.isAnimating || !this.isScrolling) return;
    this.isScrolling = false;
    this.settle();
  };

  private clearIdleTimer(): void {
    if (this.idleTimer == null) return;
    clearTimeout(this.idleTimer);
    this.idleTimer = null;
  }

  /*
   * The reader took over. An animated command stops where it is and its landing is cancelled.
   */
  private beginGesture(): void {
    this.stop();
    this.list.gestureBegan();
  }

  /*
   * The list came to rest. With snapToItem it moves to the nearest row edge first.
   */
  private settle(): void {
    const offset = this.list.offset;
    const snapped = this.list.snapTarget(offset);
    if (Math.abs(snapped - offset) >= 1) {
      this.start(snapped, SETTLE_DURATION_MS);
      return;
    }
    this.list.scrollingEnded();
  }

  /*
   * Animate to an offset for a scroll command. It ends in list.scrollingEnded.
   */
  animateTo(target: () => number): void {
    this.start(target(), COMMAND_DURATION_MS, target);
  }

  /*
   * The core moved the offset to hold the visible content in place. The animation moves with
   * it, which keeps the content it is on its way to where it was.
   */
  offsetCorrected(delta: number): void {
    if (!this.isAnimating) return;
    this.animationFrom += delta;
    this.animationTo += delta;
  }

  private start(
    target: number,
    durationMs: number,
    live: (() => number) | null = null
  ): void {
    this.stop();
    this.clearIdleTimer();
    this.isScrolling = false;
    this.isAnimating = true;
    this.animationTarget = live;
    this.animationFrom = this.list.offset;
    this.animationTo = target;
    this.animationStart = performance.now();
    this.animationDuration = durationMs;
    this.animationFrame = requestAnimationFrame(this.step);
  }

  /*
   * One frame of an animation, decelerating toward the target.
   */
  private step = (now: number): void => {
    this.animationFrame = 0;
    // A write from the page since the last step takes over, like a wheel does.
    if (this.list.movedElsewhere) {
      this.isAnimating = false;
      this.list.gestureBegan();
      return;
    }
    const progress = Math.min(
      1,
      (now - this.animationStart) / this.animationDuration
    );
    const eased = 1 - Math.pow(1 - progress, 3);
    const target = this.animationTarget?.() ?? Number.NaN;
    if (!Number.isNaN(target)) this.animationTo = target;
    this.list.scrollStep(
      this.animationFrom + (this.animationTo - this.animationFrom) * eased
    );
    if (progress < 1) {
      this.animationFrame = requestAnimationFrame(this.step);
      return;
    }
    this.isAnimating = false;
    this.list.scrollingEnded();
  };

  stop(): void {
    if (this.animationFrame !== 0) cancelAnimationFrame(this.animationFrame);
    this.animationFrame = 0;
    this.isAnimating = false;
  }
}
