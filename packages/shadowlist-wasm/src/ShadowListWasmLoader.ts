export interface ShadowListWasmLoadOptions {
  source?: BufferSource | WebAssembly.Module;
  locateFile?: (path: string, scriptDirectory: string) => string;
}

export interface ShadowListWasmModuleOptions {
  locateFile?: (path: string, scriptDirectory: string) => string;
  instantiateWasm?: (
    imports: WebAssembly.Imports,
    done: (instance: WebAssembly.Instance, module: WebAssembly.Module) => void
  ) => object;
}

/*
 * Both optional modules load once, share in-flight attempts, and forget failed attempts.
 * A source, bytes or a compiled module, instantiates through Emscripten's instantiateWasm
 * callback, which every Emscripten version reads. Emscripten 7 no longer reads wasmBinary by
 * default and would load its own file instead. The callback has no failure result: a source
 * that fails to instantiate rejects through fail.
 */
export class ShadowListWasmLoader<T> {
  private loading: Promise<void> | null = null;

  constructor(
    private readonly create: (
      options: ShadowListWasmModuleOptions
    ) => Promise<T>,
    private readonly install: (module: T) => void
  ) {}

  load(options?: ShadowListWasmLoadOptions): Promise<void> {
    if (this.loading) return this.loading;
    const attempt = new Promise<T>((resolve, reject) => {
      const source = options?.source;
      let moduleOptions: ShadowListWasmModuleOptions;
      if (source !== undefined) {
        moduleOptions = {
          instantiateWasm: (imports, done) => {
            instantiate(source, imports).then(
              ({ instance, module }) => done(instance, module),
              reject
            );
            return {};
          },
        };
      } else {
        moduleOptions = { locateFile: options?.locateFile };
      }
      this.create(moduleOptions).then(resolve, reject);
    })
      .then(this.install)
      .catch((error: unknown) => {
        if (this.loading === attempt) this.loading = null;
        throw error;
      });
    this.loading = attempt;
    return attempt;
  }
}

async function instantiate(
  source: BufferSource | WebAssembly.Module,
  imports: WebAssembly.Imports
): Promise<{ instance: WebAssembly.Instance; module: WebAssembly.Module }> {
  if (source instanceof WebAssembly.Module) {
    return {
      instance: await WebAssembly.instantiate(source, imports),
      module: source,
    };
  }
  return WebAssembly.instantiate(source, imports);
}
