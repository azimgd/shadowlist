import type { TurboModule, CodegenTypes } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

/*
 * One frame of a keyboard animation, sent by native as it runs. All values are in dp.
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
