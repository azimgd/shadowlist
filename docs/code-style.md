# Code style

Names use lower camelCase for locals and parameters, `UPPER_SNAKE_CASE` for constants and macros, and PascalCase for types and components. Members keep each language's marker: C++ `member_`, Objective-C `_member`, Java `mMember`. One word per concept in every language: `previous` (not `prev`, `old`, `last`), `current`, `index`, `count`, `offset`, `element` for a core slot, `anchor` for what MVCP holds in place, and `xDp` / `xPx` for units. Public API names, framework parameter names (`oldProps`, `oldScrollX`) and trace log keys stay as they are. `item` stays only in the public JS API, a leading `_` marks an unused JS binding, and names local to a macro take a prefix such as `slt_a`. Visible means any pixel on screen, viewable means past the threshold.

Comments are short narration of what happens, not explanations of how. A comment longer than one line, or any comment above a declaration, is a `/* */` block with one `*` per line. A one-line comment inside a body stays `//`. Delete a comment when the code already says it.

New code follows the file and siblings it lives next to: include and import order, initializer style, file layout and export style. Shared logic goes in `packages/shadowlist-core/host` and the iOS, Android and Fabric adapters call it rather than copying it. Unused code is deleted, not commented out. Commits are a single conventional line such as `fix:` or `refactor:`.

Use named constants, never bare numbers for event types or slots. Enums mirrored across C++, Objective-C and Java, such as out slots and state keys, keep the same names and numbers. Removing a feature removes its docs, package entries, trace macros, includes and test helpers too. Edit `packages/shadowlist-core` only, never its mirror in `shadowlist-fabric`.
