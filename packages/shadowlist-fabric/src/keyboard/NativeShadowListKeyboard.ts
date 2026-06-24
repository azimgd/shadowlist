import type { TurboModule, CodegenTypes } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

// A single frame of a keyboard transition, reported continuously by the native observer.
// All values are in dp.
export type KeyboardMoveEvent = {
  height: number;
  progress: number;
};

export interface Spec extends TurboModule {
  // Start/stop the native keyboard observer. Reference-counted by consumers; safe to call repeatedly.
  setEnabled(enabled: boolean): void;

  // Fires for every frame of a keyboard transition.
  readonly onKeyboardMove: CodegenTypes.EventEmitter<KeyboardMoveEvent>;
}

export default TurboModuleRegistry.get<Spec>('ShadowListKeyboard');
