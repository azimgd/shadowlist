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
  setEnabled(enabled: boolean): void;

  readonly onKeyboardMove: CodegenTypes.EventEmitter<KeyboardMoveEvent>;
}

export default TurboModuleRegistry.get<Spec>('ShadowListKeyboard');
