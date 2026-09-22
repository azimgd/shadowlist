# Variable naming standard

The repository-wide inventory is generated with:

```sh
yarn extract:variables
```

The command scans every package JavaScript/TypeScript and native source file that git tracks or would track (so the gitignored core mirror in `shadowlist-fabric/shadowlist-core` and build output are skipped) and writes `reports/variable-names.tsv`. The report lists each identifier with its standardized form, source location, language and declaration kind. A row is `needs-standardization` when the name breaks a casing rule or uses a word from the vocabulary table below. The command fails if the inventory has fewer than 2,000 names.

## Casing

- JavaScript/TypeScript locals, parameters and bindings use lower camelCase.
- JavaScript/TypeScript module constants use UPPER_SNAKE_CASE. Component-valued and type-valued constants stay PascalCase.
- Destructured props that hold a component (`ListHeaderComponent`, `ItemSeparatorComponent`) keep their PascalCase prop name, because JSX renders only capitalized identifiers.
- A leading underscore on a JavaScript/TypeScript binding (`_error`, `_id`) marks it as intentionally unused.
- C++, Objective-C, Swift, Kotlin and Java locals and parameters use lower camelCase.
- Native constants and macros use UPPER_SNAKE_CASE.
- Native member markers stay per language: C++ `member_`, Objective-C ivars `_member`, Java fields `mMember`. They encode ownership and avoid clashes with accessors.
- Names local to a macro body use a namespace prefix (`slt_a` in `CHECK_EQ`) so they cannot capture a caller's variable. They are exempt from casing.
- Parameters of framework overrides keep the framework's names: `oldProps` and `oldState` in `updateProps:oldProps:` and `updateState:oldState:`, `oldScrollX` in `onScrollChanged`, `oldLeft` in `onLayoutChange`.
- Public framework and React Native spellings stay as they are: `testID`, `nativeID`, `URL` in Apple APIs, `DIP` in React Native's Android `PixelUtil`.

## Vocabulary: one name per concept

Use the same word for the same concept in every language. Casing follows the language rules above.

| Concept                                                             | Use                                                                  | Not                                                                      |
| ------------------------------------------------------------------- | -------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| Value from before a change (previous frame, props, state, revision) | `previous`, `previousX`                                              | `prev`, `old`, `last` for this meaning                                   |
| React state-updater parameter (`setX((previous) => ...)`)           | `previous`                                                           | `prev`, `current`                                                        |
| Value at this moment                                                | `current`, `currentX`                                                | `cur`, `curr`                                                            |
| Position in a list                                                  | `index`, `xIndex`                                                    | `idx`, `pos`                                                             |
| Number of things                                                    | `count`, `xCount`                                                    | `cnt`, `num`                                                             |
| Scroll or layout distance along an axis                             | `offset`                                                             | `position` (`viewPosition` is the RN 0..1 fraction, a different concept) |
| Density-independent pixel suffix                                    | `xDp`                                                                | `xDip`                                                                   |
| Device pixel suffix                                                 | `xPx`                                                                |                                                                          |
| Visible frame of the list                                           | `window` (core), `viewport` only for a computed `[start, end)` range |                                                                          |
| Core slot for one data entry                                        | `element`                                                            | `cell`, `item` (in core and native code)                                 |
| Row synthesized by `ShadowListNative`                               | `row`                                                                |                                                                          |
| Anchor that holds content in place during MVCP                      | `anchor`                                                             | `pivot`                                                                  |

`item` stays the name for user data in the public JS API (`renderItem`, `ItemSeparatorComponent`). `visible` and `viewable` are separate concepts: visible means any pixel on screen, viewable means past `viewablePercentThreshold`. Both are public props.

The extractor flags `prev`, `cur`, `curr`, `idx`, `cnt`, `len`, `pos`, `tmp`, `dip`, `btn`, `cfg`, `evt` and `msg` as words. The list lives in `VOCABULARY` in `scripts/extract-variable-names.mjs`.

## What not to rename

- Public API: exported symbols, component props, codegen spec fields, and JSI or event payload keys. Propose these renames. Do not apply them.
- Trace log keys (`prevElements=`, `curOffset=`, `hdr=`) inside `SL_LOG`, `SLF_TRACE` and `slLog` strings. `packages/shadowlist-core-bench/ios-trace/analyze.py` and saved traces parse them.
- Names that already follow this standard, even if another spelling reads better.
