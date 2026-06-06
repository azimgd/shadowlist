import type { TurboModule, CodegenTypes } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

// A single frame of a keyboard transition, reported continuously by the native observer.
// All values are in dp.
export type KeyboardMoveEvent = {
  // Keyboard height overlapping the window, in dp: 0 closed, full height open, intermediate mid-transition.
  height: number;
  // Transition progress, 0 (closed) .. 1 (fully open).
  progress: number;
};

export interface Spec extends TurboModule {
  // Start/stop the native keyboard observer. Reference-counted by consumers; safe to call repeatedly.
  setEnabled(enabled: boolean): void;

  // Fires for every frame of a keyboard transition.
  readonly onKeyboardMove: CodegenTypes.EventEmitter<KeyboardMoveEvent>;
}

// `get` (not `getEnforcing`): returns null when the native module is absent; consumers handle null.
export default TurboModuleRegistry.get<Spec>('ShadowlistKeyboard');
