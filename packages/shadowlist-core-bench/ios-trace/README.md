# iOS simulator trace

Scripted simulator scenarios, traced frame by frame and reduced to findings about jumps, blanks and scroll jitter.
Positions and sizes are in points; item keys identify data across frames, rows are layout entries and cells are
mounted views.

## Requirements

Use macOS with Xcode and an installed iOS simulator runtime, Node from [`.nvmrc`](../../../.nvmrc), the repo's Yarn,
Ruby/Bundler, Python 3 and `agent-device` on `PATH`. The scripts launch the installed React Native example
(`shadowlist.example`); they do not build or install it.

## Build and install

From the repo root, install dependencies and select an available simulator:

```sh
yarn install
cd templates/shadowlist-fabric-example
bundle install
cd ios
bundle exec pod install
xcrun simctl list devices available
agent-device devices
export UDID='<simulator-udid>' DEVICE='<simulator-name>'
xcrun simctl boot "$UDID"          # skip if already booted
xcrun simctl bootstatus "$UDID" -b
```

Choose the same simulator in both lists. Step coordinates assume an iPhone 16 Pro (402×874pt); adapt them for other
sizes. Export `UDID` and `DEVICE` in each terminal used for tracing. The default values in `run.sh` (`sl-iosfix`)
and `perf-suite.sh` (`sl-bench`) refer to the author's local simulators.

Still in `templates/shadowlist-fabric-example/ios`, build Release with tracing compiled in and install it:

```sh
xcodebuild -workspace ShadowListExample.xcworkspace -scheme ShadowListExample \
  -configuration Release -sdk iphonesimulator -destination "id=$UDID" \
  -derivedDataPath build/dd-release \
  'GCC_PREPROCESSOR_DEFINITIONS=$(inherited) SHADOWLIST_FRAME_TRACE_COMPILED=1' build
xcrun simctl install "$UDID" build/dd-release/Build/Products/Release-iphonesimulator/ShadowListExample.app
```

Release bundles JavaScript and keeps `[SLF]`, `[SLJ]` and `[SLC]`. Rebuild and reinstall after JS changes; use Release
for timing comparisons. For Debug, use `-configuration Debug`, a separate DerivedData directory and its
`Debug-iphonesimulator` app, then keep `yarn workspace shadowlist-example start` running from the repo root.

## Run one

From the repo root, with the same exported `UDID` and `DEVICE`:

```sh
cd packages/shadowlist-core-bench/ios-trace
./run.sh chat-history my-label
```

`./run.sh <scenario> [label]` accepts a name from [scenarios](scenarios) or a path to a step file; the label defaults
to `run`. It launches with `SHADOWLIST_FRAME_TRACE=1`, drives steps with `agent-device` and prints the analyzer summary.
The log lands in `../results/ios-trace/<label>.<scenario>.log` (gitignored), markers in `<log>.marks` and JSON in
`<log>.json`. Reusing a label/scenario overwrites those files.

| Override | Purpose |
| --- | --- |
| `UDID`, `DEVICE` | simctl identifier and matching `agent-device` simulator name |
| `SESSION` | `agent-device` session; defaults to `sltrace-$DEVICE` |
| `LAUNCH_ARGS` | Extra app arguments, e.g. `"-SLCount 1000 -SLLatency 0,0"` |
| `OUT_DIR`, `LOG` | Output directory or exact log path; `LOG` takes precedence |
| `AD` | `agent-device` executable path |
| `FLINGS`, `BACK`, `HALF` | Step placeholders; defaults are 12, three quarters of `FLINGS`, and half of `FLINGS` |

For example: `LAUNCH_ARGS="-SLCount 1000" FLINGS=8 ./run.sh perf-chat my-label`.

## Step files

One step per line, `#` comments ignored:

```
route Chat            screen to open (launch arg -SLRoute), first
latency 250,700       fake network latency range in ms (-SLLatency)
wait 1.5              seconds
mark load-burst       writes a [SCN] marker into the trace
press X Y             tap (debug header buttons, shown by -SLDebug 1: prepend 210 78, append 262 78, scroll-to-random 317 78)
statusbar             tap the status bar: scroll to top
pan X Y DX DY MS      one-finger drag; negative DY scrolls a vertical list forward
repeat N <step>       run a step N times
ad <args...>          any other agent-device command in the session (e.g. ad orientation landscape)
```

Keep coordinates inside the screen or `agent-device` rejects the gesture. `{FLINGS}`, `{BACK}` and `{HALF}` in a step
expand from the environment overrides above.

## Reading the trace

- `[SLF] ... id=<listTag> frame ax=<v|h> inv= off= cs= vp= ins= ph= ref= hdr=<screenY>+<h> stt= jump= rows=[ key@screenY+height ]`
  one line per committed frame whose picture changed; positions are along the scroll axis,
  `ph` is 0 idle / 1 finger / 2 momentum / 3 scroll to top, `~` marks a concealed row.
- `[SLF] ... ev=state|drag-begin|drag-end|decel-end|stt-*|refresh-*|cmd-scroll-to-index` — every
  place the host moved the view or published state. `enabled=1` means the core wrote an offset.
- `[SLJ] ... render|vis|vis apply|reached|refresh|row-miss` — the JS side, printed natively
  through `globalThis.__shadowlistTrace` so it shares the host's clock.
- `[SL] ...` — the C++ core, without timestamps; the analyzer stamps them with the previous
  timestamped line. They can read up to a second early. File order is right. The core log is
  off by default. Turn it on with `SHADOWLIST_DEBUG_LOG=1 pod install` in the example's `ios/`
  folder, or add `SHADOWLIST_DEBUG_LOG=1` to the `GCC_PREPROCESSOR_DEFINITIONS` override of the
  xcodebuild command. Debug builds have `[SLF]` and `[SLJ]` without it.

## Findings

`python3 analyze.py <log>` prints counters for each list, then a timeline. `!!` marks an unexplained finding.

| Kind | Meaning |
| --- | --- |
| `idle-shift` | visible rows moved while nothing was driving the list: the jump a reader sees |
| `reversal` | content flipped direction for one frame (jitter), or moved backwards during a scroll to top |
| `reflow` | rows moved relative to each other with nothing before them resizing |
| `blank` | uncovered span in the viewport (default >= 120pt) |
| `discontinuity` | consecutive frames share no rows |

Expected cases are tagged `(explained)`: a fast fling, an inverted list pinned to its bottom
as messages arrive, the scroll to top jump, a host scroll command (`scrollToIndex` /
`scrollToEnd`), an overscroll settling, the refresh inset.

Builds with the core log also log the core's corrections: `[SL]   op arrived: type= key= index=` when a
correction reaches its target (type 0 MVCP, 1 scrollToIndex, 2 scrollToStart, 3 scrollToEnd,
4 bottom pin, 5 shrink clamp), `op replaced:` when one correction takes over from another, and
`anchor fallback:` when a data change removed the anchored row and MVCP holds the next visible one.

Per list it also reports `host:` corrections (how often the core moved the view), `js:` render
count, `jsms` (JS commit time) and `mountms` (render to the frame that showed it).

| Flag | What it does |
| --- | --- |
| `--window 12.3:12.8` | Prints the raw lines of that stretch |
| `--blank-pt N` | Smallest uncovered span reported as `blank` |
| `--min-shift N` | Smallest idle shift reported, in points |
| `--timeline N` | Most timeline lines printed (150) |
| `--json <path>` | Also writes the report as JSON there |

## Perf suite

From `packages/shadowlist-core-bench/ios-trace`, with the same exported simulator settings:

```sh
../perf-suite.sh ios my-label
../perf.py compare label-before my-label
```

The suite runs Feed, Chat, SectionList and Masonry at 50, 200 and 1000 items, three runs each. Override `SCREENS`,
`COUNTS`, `RUNS` or `FLINGS` to narrow it. Reports land in `../results/perf/<label>/ios/`, including `summary.md`.
`[SLC] t= commit list=<tag>` counts React Native Fabric commits in trace builds. The suite also accepts `android`
for flings driven through adb; see the [script prerequisites and options](../perf-suite.sh).
