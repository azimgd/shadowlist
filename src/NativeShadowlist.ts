import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

declare global {
  var __NATIVE_registerContainerNode: (node: any) => void;
  var __NATIVE_unregisterContainerNode: (node: any) => void;
  var __NATIVE_registerElementNode: (node: any) => void;
  var __NATIVE_unregisterElementNode: (node: any) => void;
  var __NATIVE_getRegistryElementMapping: (tag: number | string) => number;
}

export interface Spec extends TurboModule {
  setup(): void;
}

export default TurboModuleRegistry.getEnforcing<Spec>('Shadowlist');
