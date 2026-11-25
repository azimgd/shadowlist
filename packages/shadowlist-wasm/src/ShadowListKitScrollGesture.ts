import { ShadowListKitCore, easeOut } from './ShadowListKitCore.js';
import type { ShadowListKitListView } from './ShadowListKitListView.js';

type SettlingMotion = {
  kind: 'scrollend' | 'quiet' | 'wheel-quiet' | 'ended' | 'wheel-ended';
  idleTimer: ReturnType<typeof setTimeout> | null;
};

type ScrollMotion = { kind: 'still' | 'wheel-pending' } | SettlingMotion;

type ScrollState =
  | { kind: 'rest' | 'wheel-pending' | 'animating' | 'tracking-animation' }
  | { kind: 'tracking'; motion: ScrollMotion }
  | { kind: 'settling'; motion: SettlingMotion };

/*
 * The browser scrolls the list. This follows what moves it to tell the core the scroll phase:
 * a finger or the scrollbar on it is dragging and momentum is settling. Animated commands run
 * in the idle phase, as in the core's ScrollPhysics.
 * Animated commands and the snap after a scroll run here, one step per animation frame.
 */
export class ShadowListKitScrollGesture {
  private state: ScrollState = { kind: 'rest' };

  get isTracking(): boolean {
    return (
      this.state.kind === 'tracking' || this.state.kind === 'tracking-animation'
    );
  }

  get isAnimating(): boolean {
    return (
      this.state.kind === 'animating' ||
      this.state.kind === 'tracking-animation'
    );
  }

  /*
   * A held scroll still reports movement so release can announce deceleration.
   */
  get isSettling(): boolean {
    const motion = this.motion;
    return motion.kind !== 'still' && motion.kind !== 'wheel-pending';
  }

  private get motion(): ScrollMotion {
    if ('motion' in this.state) return this.state.motion;
    return {
      kind: this.state.kind === 'wheel-pending' ? 'wheel-pending' : 'still',
    };
  }

  private get wheeling(): boolean {
    const kind = this.motion.kind;
    return (
      kind === 'wheel-pending' ||
      kind === 'wheel-quiet' ||
      kind === 'wheel-ended'
    );
  }

  private get firesScrollend(): boolean {
    return typeof window !== 'undefined' && 'onscrollend' in window;
  }

  /*
   * The phase owns its pending completion. Changing it cancels or transfers that work here.
   * Quiet completion is only for wheels and browsers without scrollend.
   */
  private transition(
    state: ScrollState,
    wait: 'keep' | 'none' | 'quiet' | 'end' = 'keep'
  ): void {
    const previous = this.motion;
    const next = 'motion' in state ? state.motion : null;
    const timer = 'idleTimer' in previous ? previous.idleTimer : null;
    if (
      timer !== null &&
      (wait !== 'keep' || !next || !('idleTimer' in next))
    ) {
      clearTimeout(timer);
    }
    if (next && 'idleTimer' in next) {
      next.idleTimer = wait === 'keep' ? timer : null;
      if (wait === 'quiet' || wait === 'end') {
        next.idleTimer = setTimeout(
          this.onIdle,
          wait === 'quiet' ? ShadowListKitCore.WHEEL_IDLE_MS : 0
        );
      }
    }
    this.state = state;
  }

  private stateFor(
    motion: ScrollMotion,
    tracking = this.isTracking
  ): ScrollState {
    if (tracking) return { kind: 'tracking', motion };
    if ('idleTimer' in motion) return { kind: 'settling', motion };
    return { kind: motion.kind === 'wheel-pending' ? 'wheel-pending' : 'rest' };
  }

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
    this.transition({ kind: 'rest' });
  }

  /*
   * The browser scrolled the list for the reader. The list rests when scrollend arrives. A wheel
   * scroll, or any scroll in a browser without scrollend, rests once no scroll event follows
   * for a while.
   */
  userScrolled(): void {
    const kind = this.wheeling
      ? 'wheel-quiet'
      : this.firesScrollend
        ? 'scrollend'
        : 'quiet';
    this.transition(
      this.stateFor({ kind, idleTimer: null }),
      kind === 'scrollend' ? 'none' : 'quiet'
    );
  }

  /*
   * A finger on a list in motion stops it. The list rests once every finger lifted.
   */
  private onTouchStart = (): void => {
    this.beginGesture();
    this.track('none');
    this.list.draggingBegan();
  };

  private onTouchEnd = (event: TouchEvent): void => {
    if (event.touches.length > 0) return;
    this.endTracking();
    this.list.draggingEnded(this.isSettling);
    this.release();
  };

  /*
   * The finger or the mouse let go. One never moved rests now, as does one whose scrollend
   * already came. A list the reader moved rests at the scrollend of its momentum or of the
   * lift. Without scrollend it rests once no scroll event follows for a while, which also
   * covers a lift that left the list still.
   */
  private release(): void {
    if (!this.isSettling) {
      this.settle();
      return;
    }
    if (this.motion.kind === 'ended' || this.motion.kind === 'wheel-ended') {
      this.onIdle();
      return;
    }
    this.transition(
      this.state,
      this.wheeling || !this.firesScrollend ? 'quiet' : 'none'
    );
  }

  /*
   * A mouse press on the list itself, not on a row, is on its scrollbar.
   */
  private onPointerDown = (event: PointerEvent): void => {
    if (event.pointerType !== 'mouse' || event.target !== this.list) return;
    this.beginGesture();
    this.track('keep');
    this.list.draggingBegan();
  };

  private onPointerUp = (event: PointerEvent): void => {
    if (event.pointerType !== 'mouse' || !this.isTracking) return;
    this.endTracking();
    this.list.draggingEnded(this.isSettling);
    this.release();
  };

  private onWheel = (): void => {
    this.previousWheel = performance.now();
    this.beginGesture();
    const motion = this.motion;
    this.transition(
      this.stateFor(
        'idleTimer' in motion
          ? {
              kind:
                motion.kind === 'ended' || motion.kind === 'wheel-ended'
                  ? 'wheel-ended'
                  : 'wheel-quiet',
              idleTimer: null,
            }
          : { kind: 'wheel-pending' }
      )
    );
  };

  /*
   * A scrollend right after a wheel step is not the end. The idle timer ends the scroll once
   * the wheel stopped, which keeps a snap from pulling against the wheel.
   */
  private onScrollEnd = (): void => {
    if (this.isAnimating || !this.isSettling) return;
    this.transition(
      this.stateFor({
        kind: this.wheeling ? 'wheel-ended' : 'ended',
        idleTimer: null,
      })
    );
    if (
      performance.now() - this.previousWheel <
      ShadowListKitCore.WHEEL_IDLE_MS
    )
      return;
    /*
     * Rest once this frame is done. A page that writes the offset on every frame gets the
     * scrollend of one write in the frame of the next, and a snap started now would move the
     * rows before that frame paints. A write that follows stops the snap before its first step.
     */
    this.transition(this.state, 'end');
  };

  private onIdle = (): void => {
    this.transition(this.state, 'none');
    if (this.isTracking || this.isAnimating || !this.isSettling) return;
    this.transition({ kind: 'rest' });
    this.settle();
  };

  private track(wait: 'keep' | 'none'): void {
    const motion = this.motion;
    this.transition(
      this.stateFor(
        'idleTimer' in motion
          ? {
              kind:
                motion.kind === 'ended' || motion.kind === 'wheel-ended'
                  ? 'ended'
                  : this.firesScrollend
                    ? 'scrollend'
                    : 'quiet',
              idleTimer: null,
            }
          : { kind: 'still' },
        true
      ),
      wait
    );
  }

  private endTracking(): void {
    this.transition(
      this.isAnimating
        ? { kind: 'animating' }
        : this.stateFor(this.motion, false)
    );
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
      this.start(snapped, ShadowListKitCore.SETTLE_DURATION_MS);
      return;
    }
    this.list.scrollingEnded(false);
  }

  /*
   * Animate to an offset for a scroll command. It ends in list.scrollingEnded, as a command.
   */
  animateTo(target: () => number): void {
    this.start(target(), ShadowListKitCore.COMMAND_DURATION_MS, target);
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
    this.transition({
      kind: this.isTracking ? 'tracking-animation' : 'animating',
    });
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
      this.transition(this.stateFor({ kind: 'still' }));
      this.list.gestureBegan();
      return;
    }
    const progress = Math.min(
      1,
      (now - this.animationStart) / this.animationDuration
    );
    const eased = easeOut(progress);
    const target = this.animationTarget?.() ?? Number.NaN;
    if (!Number.isNaN(target)) this.animationTo = target;
    this.list.scrollStep(
      this.animationFrom + (this.animationTo - this.animationFrom) * eased
    );
    if (progress < 1) {
      this.animationFrame = requestAnimationFrame(this.step);
      return;
    }
    this.transition(this.stateFor({ kind: 'still' }));
    this.list.scrollingEnded(this.animationTarget !== null);
  };

  stop(): void {
    if (this.animationFrame !== 0) cancelAnimationFrame(this.animationFrame);
    this.animationFrame = 0;
    if (this.isAnimating) this.transition(this.stateFor({ kind: 'still' }));
    this.animationTarget = null;
  }
}
