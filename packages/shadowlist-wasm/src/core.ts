/*
 * Loads the virtualization core and hands out fresh per-list instances.
 */
import createShadowlistCoreModule, {
  type ShadowlistCoreInstance,
  type ShadowlistCoreModule,
} from './wasm/shadowlistCore.js';

export type {
  ShadowlistCoreInstance,
  ElementLayout,
  VisibleIndices,
  StateUpdate,
} from './wasm/shadowlistCore.js';

let modulePromise: Promise<ShadowlistCoreModule> | null = null;

/*
 * Initialize (or reuse) the shared core module. Safe to call repeatedly.
 */
export function loadShadowlistCoreModule(): Promise<ShadowlistCoreModule> {
  if (!modulePromise) {
    modulePromise = createShadowlistCoreModule();
  }
  return modulePromise;
}

/*
 * Create a new core handle for a single list. Call instance.delete() on
 * unmount to free the native resources.
 */
export async function createShadowlistCore(): Promise<ShadowlistCoreInstance> {
  const module = await loadShadowlistCoreModule();
  return new module.ShadowlistCore();
}
