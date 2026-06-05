import type { TurboModule, CodegenTypes } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

/*
 * A single frame of a keyboard transition, reported continuously by the native
 * observer (Android WindowInsetsAnimation every frame; iOS keyboard notifications +
 * CADisplayLink for animated open/close and every keyboardWillChangeFrame during an
 * interactive drag). All values are in dp.
 */
export type KeyboardMoveEvent = {
  /*
   * Current keyboard height overlapping the app window, in dp. 0 when fully closed,
   * the full keyboard height when fully open, and intermediate values mid-transition
   * (including while the user drags the keyboard down interactively).
   */
  height: number;
  /*
   * Transition progress, 0 (closed) .. 1 (fully open). Derived from height / target,
   * so it tracks the interactive drag too.
   */
  progress: number;
};

export interface Spec extends TurboModule {
  /*
   * Start/stop the native keyboard observer. Reference-counted on the JS side
   * (useKeyboardAnimation), so the observer runs only while at least one consumer is
   * mounted. Safe to call repeatedly.
   */
  setEnabled(enabled: boolean): void;

  /*
   * Fires for every frame of a keyboard transition. The native side drives this; on
   * Android it is the genuine per-frame inset, on iOS it is the real frame during
   * interactive drags and a duration/curve-matched interpolation during the animated
   * open/close (see the module's platform notes).
   */
  readonly onKeyboardMove: CodegenTypes.EventEmitter<KeyboardMoveEvent>;
}

/*
 * `get` (not `getEnforcing`): returns null when the native module is not present in
 * the build, so importing the library never crashes a JS-only reload before a native
 * rebuild. Consumers (useKeyboardAnimation) degrade gracefully when it is null.
 */
export default TurboModuleRegistry.get<Spec>('ShadowlistKeyboard');
