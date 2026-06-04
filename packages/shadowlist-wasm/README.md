# shadowlist-wasm

WebAssembly virtualization for **React on the web**, powered by the exact same
`shadowlist-core` C++ algorithm that drives the React Native (Fabric)
integration. The core is compiled to WASM with Emscripten + embind, and a thin
react-dom layer drives it through the same per-frame cycle every integration
uses: reconcile keys → measure → feed measured DOM sizes back → resolve scroll
corrections → publish content size.

Because the algorithm is shared, the web list behaves identically to native:
default / inverted / nested / multi-column layouts, append / prepend with
maintain-visible-content-position anchoring, imperative `scrollToIndex`, and
concurrent updates.

## Layout

```
cpp/Binding.cpp            embind wrapper around Container + Virtualizer
scripts/build-wasm.sh      compiles ../shadowlist-core + the binding to WASM
src/wasm/shadowlistCore.*  generated ES module + .wasm (checked in)
src/core.ts                loads the WASM module, hands out per-list instances
src/Shadowlist.tsx         the react-dom <Shadowlist> component
src/types.ts               public props/commands (aligned with the RN package)
example/                   Vite app: Feed, Chat, Nested, Masonry, Contacts
```

## Building the WASM core

Requires the [Emscripten SDK](https://emscripten.org/). Point `EMSDK` at its
root (defaults to `~/emsdk`) or have `emcc` on `PATH`, then:

```sh
yarn build:wasm   # -> src/wasm/shadowlistCore.{js,wasm}
```

The generated artifacts are committed, so consumers and the example do not need
Emscripten installed — only rebuild when `shadowlist-core` or the binding change.

## Running the example

```sh
cd example
npm install
npm run dev      # http://localhost:5173
```

The example resolves `shadowlist-wasm` straight from `../src` via a Vite alias,
so it always tracks the in-repo source.

## Usage

```tsx
import { Shadowlist, type ShadowlistCommands } from 'shadowlist-wasm';

const ref = useRef<ShadowlistCommands>(null);

<Shadowlist
  ref={ref}
  data={data}                       // ReadonlyArray<{ id: string; ... }>
  style={{ flex: 1 }}
  renderElement={({ element, index }) => <Row element={element} index={index} />}
  inverted={false}
  horizontal={false}
  columns={1}
  onEndReached={loadMore}
  onScroll={({ nativeEvent }) => {}}
  ListHeaderComponent={<Header />}
  ListFooterComponent={<Footer />}
  ListEmptyComponent={<Empty />}
/>

ref.current?.scrollToIndex(500);
ref.current?.setEndReachedEnabled(false);
```

The props and the imperative command surface intentionally mirror the React
Native `shadowlist` package so application code is portable between platforms.
