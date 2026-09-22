# ShadowList

ShadowList is a monorepo with one shared list-virtualization engine and the React Native package that uses it.

## Packages

- `shadowlist-core`
  Shared C++ virtualization engine used by every integration.

- `shadowlist`
  React Native Fabric list components: `ShadowList`, `SectionList`, `TreeList`, `DraggableList`
  and `ShadowListNative` (rows built natively from templates, see
  [SHADOWLIST_NATIVE.md](packages/shadowlist-fabric/SHADOWLIST_NATIVE.md)).

## Repo Layout

```text
packages/shadowlist-core            Shared C++ core
packages/shadowlist-core-tests      Core integration and perf tests
packages/shadowlist-core-bench      Core micro-benchmarks and device metrics scripts
packages/shadowlist-fabric          React Native (Fabric) package
packages/shadowlist-fabric-example  React Native example app
packages/shadowlist-utils           Shared demo components used by the example app
```

## Examples

- React Native example screens live in `packages/shadowlist-fabric-example/src`
