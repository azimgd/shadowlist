export interface ShadowListWasmLoadOptions {
  source?: BufferSource | WebAssembly.Module;
  locateFile?: (path: string, scriptDirectory: string) => string;
}

export interface ShadowListWasmModuleOptions {
  locateFile?: (path: string, scriptDirectory: string) => string;
  wasmBinary?: BufferSource;
  instantiateWasm?: (
    imports: WebAssembly.Imports,
    done: (instance: WebAssembly.Instance, module: WebAssembly.Module) => void
  ) => object;
}

/*
 * Both optional modules load once, share in-flight attempts, and forget failed attempts.
 * A compiled module that fails to instantiate rejects through fail: Emscripten's
 * instantiateWasm callback itself has no failure result.
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
      if (source instanceof WebAssembly.Module) {
        moduleOptions = {
          instantiateWasm: (imports, done) => {
            WebAssembly.instantiate(source, imports).then(
              (instance) => done(instance, source),
              reject
            );
            return {};
          },
        };
      } else if (source !== undefined) {
        moduleOptions = { wasmBinary: source };
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
