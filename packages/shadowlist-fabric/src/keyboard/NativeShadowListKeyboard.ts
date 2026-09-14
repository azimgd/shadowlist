import type { TurboModule, CodegenTypes } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

/*
 * A single frame of a keyboard transition, reported continuously by the native observer.
 * All values are in dp.
 */
export type KeyboardMoveEvent = {
  height: number;
  progress: number;
};

export interface Spec extends TurboModule {
  /*
   * Start/stop the native keyboard observer. The native side reference-counts calls, so
   * multiple independent consumers (e.g. several ShadowList instances) can each call
   * setEnabled(true)/setEnabled(false) without stopping the observer out from under one
   * another; the observer only actually stops once every enabled call has a matching
   * disabled call. Safe to call repeatedly and from multiple call sites.
   */
  setEnabled(enabled: boolean): void;

  // Fires for every frame of a keyboard transition.
  readonly onKeyboardMove: CodegenTypes.EventEmitter<KeyboardMoveEvent>;
}

export default TurboModuleRegistry.get<Spec>('ShadowListKeyboard');
