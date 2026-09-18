# iOS device trace

Scripted scenarios on a simulator, traced frame by frame, reduced to findings. It answers
"did the content jump / go blank / stutter, and what did the list do at that moment", which
neither a screenshot nor a frame-rate number can.

## Run one

```sh
./run.sh <scenario> <label>          # scenarios/<scenario>.steps, or a path to a step file
UDID=<udid> DEVICE=<sim name> ./run.sh chat-history mylabel
```

It launches the example app with `SHADOWLIST_FRAME_TRACE=1` through
`simctl launch --console-pty`, drives the steps with agent-device, then prints the analyzer
summary. The raw log lands in `../results/ios-trace/<label>.<scenario>.log` (gitignored), the
scenario markers next to it in `<log>.marks`, and a JSON report in `<log>.json`.

`DEVICE` is the agent-device NAME of the simulator, `UDID` its identifier; both must point at
the same device. Debug builds need Metro; a Release build carries its own bundle.

## Step files

One step per line, `#` comments ignored:

```
route Chat            screen to open (launch arg -SLRoute), first
latency 250,700       fake network latency range in ms (-SLLatency)
wait 1.5              seconds
mark load-burst       writes a [SCN] marker into the trace
press X Y             tap (prepend 296 80, append 333 80, scroll-to-random 372 80)
statusbar             tap the status bar: scroll to top
pan X Y DX DY MS      one-finger drag; negative DY scrolls a vertical list forward
repeat N <step>       run a step N times
ad <args...>          any other agent-device command in the session (e.g. ad orientation landscape)
```

Coordinates assume iPhone 16 Pro (402x874pt) and must stay inside the screen, or agent-device
rejects the gesture.

## Reading the trace

- `[SLF] ... id=<listTag> frame ax=<v|h> inv= off= cs= vp= ins= ph= ref= hdr=<screenY>+<h> stt= jump= rows=[ key@screenY+height ]`
  one line per committed frame whose picture changed; positions are along the scroll axis,
  `ph` is 0 idle / 1 finger / 2 momentum / 3 scroll-to-top, `~` marks a concealed row.
- `[SLF] ... ev=state|drag-begin|drag-end|decel-end|stt-*|refresh-*|cmd-scroll-to-index` — every
  place the host moved the view or published state. `enabled=1` means the core wrote an offset.
- `[SLJ] ... render|vis|vis apply|reached|refresh|row-miss` — the JS side, printed natively
  through `globalThis.__shadowlistTrace` so it shares the host's clock.
- `[SL] ...` — the C++ core (debug builds only), without timestamps; the analyzer stamps them
  with the previous timestamped line, so they can read up to a second early. File order is right.

## Findings

`analyze.py <log>` prints per-list counters, then a timeline. `!!` marks an unexplained finding.

| kind            | meaning                                                                                   |
| --------------- | ----------------------------------------------------------------------------------------- |
| `idle-shift`    | visible rows moved while nothing was driving the list: the jump a reader sees             |
| `reversal`      | content flipped direction for one frame (jitter), or moved backwards during scroll-to-top |
| `reflow`        | rows moved relative to each other with nothing before them resizing                       |
| `blank`         | uncovered span in the viewport (default >= 120pt)                                         |
| `discontinuity` | consecutive frames share no rows                                                          |

Expected cases are tagged `(explained)`: a fast fling, an inverted list pinned to its bottom
as messages arrive, the scroll-to-top jump, an overscroll settling, the refresh inset.

Per list it also reports `host:` corrections (how often the core moved the view), `js:` render
count, `jsms` (JS commit time) and `mountms` (render to the frame that showed it).

Useful flags: `--window 12.3:12.8` prints the raw lines of that stretch, `--blank-pt`,
`--min-shift`, `--timeline`, `--json`.

## Release trace build

Debug JS is several times slower than release, which distorts anything timing-related. Build
the example in Release with the trace compiled in:

```sh
xcodebuild -workspace ShadowListExample.xcworkspace -scheme ShadowListExample \
  -configuration Release -sdk iphonesimulator -destination id=<udid> \
  -derivedDataPath build/dd-release \
  'GCC_PREPROCESSOR_DEFINITIONS=$(inherited) SHADOWLIST_FRAME_TRACE_COMPILED=1' build
```

It keeps `[SLF]`/`[SLJ]` and drops the per-commit `[SL]` core log. Its JS is bundled, so Metro
edits do not reach it until the next build.
