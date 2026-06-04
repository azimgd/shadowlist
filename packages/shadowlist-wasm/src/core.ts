/*
 * Loads the WebAssembly virtualization core and hands out fresh per-list
 * instances. The module is a singleton (one WASM heap shared by every list);
 * each ShadowlistCore handle owns its own Container + Virtualizer.
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
 * Initialize (or reuse) the shared WASM module. Safe to call repeatedly; the
 * underlying compile happens exactly once.
 */
export function loadShadowlistCoreModule(): Promise<ShadowlistCoreModule> {
  if (!modulePromise) {
    modulePromise = createShadowlistCoreModule();
  }
  return modulePromise;
}

/*
 * Create a new core handle for a single list. Remember to call instance.delete()
 * when the list unmounts to free the C++ Container/Virtualizer.
 */
export async function createShadowlistCore(): Promise<ShadowlistCoreInstance> {
  const module = await loadShadowlistCoreModule();
  return new module.ShadowlistCore();
}
