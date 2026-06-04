# Shadowlist

Shadowlist is a monorepo for the shared list virtualization engine and its platform integrations.

## Packages

- `shadowlist-core`
  Shared C++ virtualization engine used by every integration.
  Docs: [`packages/shadowlist-core/README.md`](packages/shadowlist-core/README.md)

- `shadowlist`
  React Native Fabric list component.
  Docs: [`packages/shadowlist-fabric/README.md`](packages/shadowlist-fabric/README.md)

- `shadowlist-wasm`
  React web list component and low-level WASM bindings.
  Docs: [`packages/shadowlist-wasm/README.md`](packages/shadowlist-wasm/README.md)

## Repo Layout

```text
packages/shadowlist-core         Shared C++ core
packages/shadowlist-core-tests   Core integration tests and perf tests
packages/shadowlist-fabric       React Native Fabric package + example app
packages/shadowlist-wasm         React web / WASM package + example app
```

## Examples

- React Native example screens live in `packages/shadowlist-fabric/example/src`
- React web example screens live in `packages/shadowlist-wasm/example/src`

## Notes

- The Fabric and WASM packages aim to stay API-aligned where practical.
- Both integrations use the same core behavior for virtualization, anchored updates, and scroll correction.
