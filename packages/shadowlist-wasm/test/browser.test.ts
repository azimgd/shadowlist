/*
 * The list in headless Chrome, driven over the DevTools protocol through test/fixture.html.
 * Skipped when no Chrome is found. CHROME_PATH picks the binary. Run ./build.sh first.
 * The route test opens the web example of the shadowlist-templates repository, found at
 * SHADOWLIST_TEMPLATES_ROOT (default ../shadowlist-templates next to this repository). It is
 * skipped when that example or its build is missing.
 */
import assert from 'node:assert/strict';
import { spawn, type ChildProcess } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { createServer, type Server } from 'node:http';
import { tmpdir } from 'node:os';
import { extname, join, normalize, resolve as resolvePath } from 'node:path';
import { after, before, test } from 'node:test';

const ROOT = new URL('..', import.meta.url).pathname;
const TEMPLATES_ROOT = resolvePath(
  process.env.SHADOWLIST_TEMPLATES_ROOT ||
    new URL('../../../../shadowlist-templates', import.meta.url).pathname
);
const WEB_EXAMPLE = `${join(TEMPLATES_ROOT, 'examples/shadowlist-web-example')}/`;
// The packages of the templates repository, which the web example imports beside this one.
const TEMPLATES_PACKAGES = `${join(TEMPLATES_ROOT, 'packages')}/`;
const WEB_EXAMPLE_SKIP = !existsSync(WEB_EXAMPLE)
  ? `no web example at ${WEB_EXAMPLE}, set SHADOWLIST_TEMPLATES_ROOT to the shadowlist-templates checkout`
  : !existsSync(join(WEB_EXAMPLE, 'dist/main.js'))
    ? `the web example at ${WEB_EXAMPLE} is not built`
    : false;
let serverPort = 0;
let browserPort = 0;

const CHROME_CANDIDATES = [
  process.env.CHROME_PATH ?? '',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/google-chrome-stable',
  '/usr/bin/chromium',
  '/usr/bin/chromium-browser',
];
// SHADOWLIST_WASM_BROWSER=0 skips the browser tests.
const CHROME =
  process.env.SHADOWLIST_WASM_BROWSER === '0'
    ? undefined
    : CHROME_CANDIDATES.find((path) => path && existsSync(path));

const TYPES: Record<string, string> = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.mjs': 'text/javascript',
  '.wasm': 'application/wasm',
};

let server: Server | null = null;
let chrome: ChildProcess | null = null;
let profile = '';
let page: Page | null = null;

/*
 * One tab over its DevTools WebSocket.
 */
class Page {
  private nextId = 0;
  private readonly pending = new Map<
    number,
    { resolve: (value: unknown) => void; reject: (error: Error) => void }
  >();
  readonly errors: string[] = [];
  private readonly socket: WebSocket;

  private constructor(socket: WebSocket) {
    this.socket = socket;
    socket.addEventListener('message', (event) => {
      const message = JSON.parse(String(event.data));
      const waiting = this.pending.get(message.id);
      if (waiting) {
        this.pending.delete(message.id);
        if (message.error) waiting.reject(new Error(message.error.message));
        else waiting.resolve(message.result);
      } else if (message.method === 'Runtime.exceptionThrown') {
        this.errors.push(JSON.stringify(message.params.exceptionDetails));
      }
    });
  }

  static async open(
    debugPort: number,
    url: string,
    ready = 'window.fixtureReady === true'
  ): Promise<Page> {
    const response = await fetch(
      `http://127.0.0.1:${debugPort}/json/new?about:blank`,
      { method: 'PUT' }
    );
    const target = (await response.json()) as { webSocketDebuggerUrl: string };
    const socket = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((resolve, reject) => {
      socket.addEventListener('open', resolve);
      socket.addEventListener('error', reject);
    });
    const opened = new Page(socket);
    await opened.send('Runtime.enable');
    await opened.send('Network.setCacheDisabled', { cacheDisabled: true });
    await opened.send('Emulation.setDeviceMetricsOverride', {
      width: 800,
      height: 900,
      deviceScaleFactor: 1,
      mobile: false,
    });
    await opened.send('Page.navigate', { url });
    for (let attempt = 0; attempt < 200; ++attempt) {
      if (await opened.evaluate(ready)) return opened;
      await new Promise((resolve) => setTimeout(resolve, 25));
    }
    throw new Error(`fixture did not load: ${opened.errors.join('\n')}`);
  }

  send(method: string, params: object = {}): Promise<unknown> {
    const id = ++this.nextId;
    this.socket.send(JSON.stringify({ id, method, params }));
    return new Promise((resolve, reject) =>
      this.pending.set(id, { resolve, reject })
    );
  }

  /*
   * The value of an expression in the page, awaited when it is a promise.
   */
  async evaluate<T = unknown>(expression: string): Promise<T> {
    const result = (await this.send('Runtime.evaluate', {
      expression,
      awaitPromise: true,
      returnByValue: true,
    })) as {
      result: { value: T };
      exceptionDetails?: { exception?: { description?: string } };
    };
    if (result.exceptionDetails) {
      throw new Error(
        result.exceptionDetails.exception?.description ?? 'evaluate failed'
      );
    }
    return result.result.value;
  }

  close(): void {
    this.socket.close();
  }
}

function serve(): Promise<number> {
  server = createServer((request, response) => {
    const pathname = decodeURIComponent(
      new URL(request.url!, 'http://x').pathname
    );
    const web = pathname.startsWith('/web/');
    const packageAsset = pathname.startsWith('/packages/shadowlist-wasm/');
    const templatesPackage = !packageAsset && pathname.startsWith('/packages/');
    const root = web
      ? WEB_EXAMPLE
      : templatesPackage
        ? TEMPLATES_PACKAGES
        : ROOT;
    let relative = web
      ? pathname.slice('/web/'.length)
      : packageAsset
        ? pathname.slice('/packages/shadowlist-wasm/'.length)
        : templatesPackage
          ? pathname.slice('/packages/'.length)
          : pathname;
    if (web && relative === '') relative = 'index.html';
    const path = normalize(join(root, relative));
    if (!path.startsWith(root) || !existsSync(path)) {
      response.writeHead(404).end();
      return;
    }
    response.writeHead(200, {
      'Content-Type': TYPES[extname(path)] ?? 'application/octet-stream',
    });
    response.end(readFileSync(path));
  });
  return new Promise((resolve) =>
    server!.listen(0, '127.0.0.1', () => {
      const address = server!.address();
      resolve(typeof address === 'object' && address ? address.port : 0);
    })
  );
}

function launchChrome(): Promise<number> {
  profile = mkdtempSync(join(tmpdir(), 'shadowlist-wasm-'));
  const flags = [
    '--headless=new',
    '--remote-debugging-port=0',
    `--user-data-dir=${profile}`,
    '--no-first-run',
    '--no-default-browser-check',
    'about:blank',
  ];
  if (process.platform === 'linux') flags.unshift('--no-sandbox');
  chrome = spawn(CHROME!, flags, { stdio: ['ignore', 'ignore', 'pipe'] });
  return new Promise((resolve, reject) => {
    let output = '';
    chrome!.stderr!.on('data', (chunk: Buffer) => {
      output += chunk.toString();
      const match = /DevTools listening on ws:\/\/[^:]+:(\d+)\//.exec(output);
      if (match) resolve(Number(match[1]));
    });
    chrome!.on('exit', () => reject(new Error(`Chrome exited: ${output}`)));
  });
}

before(async () => {
  if (!CHROME) return;
  const [port, debugPort] = await Promise.all([serve(), launchChrome()]);
  serverPort = port;
  browserPort = debugPort;
  page = await Page.open(
    debugPort,
    `http://127.0.0.1:${port}/test/fixture.html`
  );
});

after(async () => {
  chrome?.removeAllListeners('exit');
  // Chrome writes its profile until it exits. Remove the profile after that.
  if (chrome && chrome.exitCode === null) {
    const exited = new Promise((resolve) => chrome!.once('exit', resolve));
    // DevTools closes Chrome after its profile is flushed, including the extra example tabs.
    page?.send('Browser.close').catch(() => {});
    await Promise.race([
      exited,
      new Promise((resolve) => setTimeout(resolve, 5000).unref()),
    ]);
    if (chrome.exitCode === null) {
      chrome.kill();
      await exited;
    }
  }
  page?.close();
  server?.close();
  if (profile) {
    rmSync(profile, {
      recursive: true,
      force: true,
      maxRetries: 10,
      retryDelay: 100,
    });
  }
});

const browserTest = (name: string, body: (tab: Page) => Promise<void>) =>
  test(name, { skip: CHROME ? false : 'no Chrome found' }, () => body(page!));

/*
 * A new list in the page and a settled first layout, scrolled to where: start, middle or end.
 */
async function makeList(
  tab: Page,
  options: string,
  where: 'start' | 'middle' | 'end' = 'start'
): Promise<void> {
  await tab.evaluate(`(async () => {
    const list = make(${options});
    await frames(3);
    if ('${where}' === 'middle') {
      if (list.horizontal) list.scrollLeft = 8000;
      else list.scrollTop = 8000;
    }
    if ('${where}' === 'end') list.scrollToEnd();
    await frames(4);
  })()`);
}

const LAYOUTS = [
  '{}',
  '{ inverted: true }',
  '{ sized: true }',
  '{ horizontal: true }',
  '{ horizontal: true, inverted: true }',
  '{ estimate: 20 }',
];

browserTest(
  'prepends, appends and deletes keep the visible rows',
  async (tab) => {
    for (const layout of LAYOUTS) {
      for (const where of ['middle', 'end'] as const) {
        await makeList(tab, layout, where);
        for (const change of [
          'list.prependRows(10)',
          'list.appendRows(10)',
          'list.prependRows(300)',
          'list.appendRows(300)',
          'list.removeFirstRows(5)',
          'list.removeLastRows(5)',
        ]) {
          const issues = await tab.evaluate(
            `watch(async () => { ${change} }, 6)`
          );
          assert.deepEqual(issues, [], `${layout} ${where} ${change}`);
        }
      }
    }
  }
);

browserTest(
  'an inverted list follows appends only with followAppends',
  async (tab) => {
    for (const follow of [false, true]) {
      await makeList(
        tab,
        `{ inverted: true, followAppends: ${follow} }`,
        'end'
      );
      const ends = await tab.evaluate<boolean[]>(`(async () => {
      const ends = [];
      for (let count = 1; count <= 4; ++count) {
        list.appendRows(count);
        await frames(2);
        ends.push(atEnd());
      }
      return ends;
    })()`);
      assert.deepEqual(ends, [follow, follow, follow, follow]);
      // Away from the end, an append never moves what is on screen.
      await tab.evaluate('list.scrollTop -= 1500; frames(3)');
      assert.deepEqual(
        await tab.evaluate('watch(async () => list.appendRows(3))'),
        []
      );
    }
  }
);

browserTest(
  'a small list grows and a large list shrinks without blanks',
  async (tab) => {
    for (const layout of ['{ count: 3 }', '{ count: 3, inverted: true }']) {
      await makeList(tab, layout);
      const gaps = await tab.evaluate(`(async () => {
      const gaps = [];
      for (let step = 0; step < 12; ++step) {
        list.appendRows(4);
        await frames(2);
        gaps.push(...blanks());
      }
      return gaps;
    })()`);
      assert.deepEqual(gaps, [], layout);
    }
    await makeList(tab, '{ count: 2000 }', 'middle');
    const held = await tab.evaluate<{ key: string; start: number }>('anchor()');
    const positions = await tab.evaluate(`(async () => {
    const positions = [];
    for (let step = 0; step < 4; ++step) {
      list.appendRows(200);
      list.prependRows(100);
      await frames(2);
      positions.push(positionOf('${held.key}'));
    }
    list.removeLastRows(list.itemCount - 1600);
    await frames(2);
    positions.push(positionOf('${held.key}'));
    list.removeFirstRows(300);
    await frames(2);
    positions.push(positionOf('${held.key}'));
    return positions;
  })()`);
    assert.deepEqual(positions, Array(6).fill(held.start));
    const shrunk = await tab.evaluate<{
      offset: number;
      rows: number;
    }>(`(async () => {
    list.removeLastRows(list.itemCount - 4);
    await frames(3);
    return { offset: list.scrollTop, rows: visible().length, gaps: blanks() };
  })()`);
    assert.deepEqual(shrunk, { offset: 0, rows: 4, gaps: [] });
  }
);

browserTest('aggressive scrolling shows no blank and no jump', async (tab) => {
  for (const layout of [
    '{}',
    '{ inverted: true }',
    '{ horizontal: true }',
    '{ columns: 3, sized: true }',
    '{ nested: true }',
    '{ estimate: 400 }',
  ]) {
    await makeList(tab, layout, 'middle');
    const issues = await tab.evaluate(`(async () => {
      const next = random(7);
      const along = list.horizontal ? list.clientWidth : list.clientHeight;
      const deltas = [
        ...Array.from({ length: 30 }, (_, step) => (step % 10 < 5 ? 1 : -1) * along),
        ...Array.from({ length: 50 }, () => Math.round((next() - 0.5) * 3 * along)),
      ];
      const issues = await scrollRun(deltas);
      // Far jumps land on rows never measured.
      for (let step = 0; step < 15; ++step) {
        const target = Math.round(next() * list.contentSize);
        if (list.horizontal) list.scrollLeft = target;
        else list.scrollTop = target;
        await frames(1);
        const gaps = blanks();
        if (gaps.length > 0) issues.push({ jump: target, gaps });
      }
      return issues;
    })()`);
    assert.deepEqual(issues, [], layout);
  }
});

browserTest(
  'a page write takes over from a running snap animation',
  async (tab) => {
    await makeList(tab, '{ horizontal: true, sized: true }');
    await tab.evaluate('list.snapToItem = true; frames(2)');
    const issues = await tab.evaluate(
      'scrollRun(Array.from({ length: 40 }, (_, step) => (step < 20 ? 40 : -40)))'
    );
    assert.deepEqual(issues, []);
    // At rest the list moves to a row edge.
    const snapped = await tab.evaluate<boolean>(`(async () => {
    list.scrollLeft = 1234;
    await new Promise((resolve) => setTimeout(resolve, 800));
    return Math.abs(list.liveCore.nearestSnapOffset(list.scrollLeft) - list.scrollLeft) < 1;
  })()`);
    assert.equal(snapped, true);
  }
);

browserTest(
  'a touch scroll rests at its scrollend without the wheel wait',
  async (tab) => {
    await makeList(tab, '{ sized: true }', 'middle');
    await tab.evaluate(`(() => {
    list.snapToItem = true;
    window.waits = 0;
    window.rests = 0;
    const plain = window.setTimeout.bind(window);
    window.plainSetTimeout ??= plain;
    window.setTimeout = (work, delay, ...rest) => {
      if (delay > 0) ++window.waits;
      return window.plainSetTimeout(work, delay, ...rest);
    };
    list.delegate = { ...list.delegate, didEndDecelerating: () => ++window.rests };
  })()`);
    await tab.send('Emulation.setTouchEmulationEnabled', { enabled: true });
    const touch = (type: string, y: number) =>
      tab.send('Input.dispatchTouchEvent', {
        type,
        touchPoints: type === 'touchEnd' ? [] : [{ x: 200, y }],
      });
    await touch('touchStart', 500);
    for (let step = 1; step <= 12; ++step) {
      await touch('touchMove', 500 - step * 23);
      await tab.evaluate('frames(1)');
    }
    await touch('touchEnd', 224);
    const result = await tab.evaluate<{
      waits: number;
      rests: number;
      snapped: boolean;
      idle: boolean;
    }>(`(async () => {
    for (let frame = 0; frame < 120 && window.rests === 0; ++frame) await frames(1);
    window.setTimeout = window.plainSetTimeout;
    return {
      waits: window.waits,
      rests: window.rests,
      snapped: Math.abs(list.liveCore.nearestSnapOffset(list.scrollTop) - list.scrollTop) < 1,
      idle: !list.gesture.isSettling && !list.gesture.isTracking,
    };
  })()`);
    await tab.send('Emulation.setTouchEmulationEnabled', { enabled: false });
    assert.deepEqual(result, { waits: 0, rests: 1, snapped: true, idle: true });
  }
);

browserTest(
  'scroll commands land on their rows without blanks',
  async (tab) => {
    for (const layout of ['{}', '{ inverted: true }', '{ horizontal: true }']) {
      await makeList(tab, layout);
      for (const [index, position, animated] of [
        [500, 0, false],
        [100, 0.5, true],
        [900, 1, true],
        [3, 0, true],
      ] as const) {
        const landed = await tab.evaluate<{
          start: number;
          expected: number;
        }>(`(async () => {
        list.scrollToItem(${index}, ${position}, ${animated});
        let gaps = 0;
        for (let frame = 0; frame < 20; ++frame) {
          await frames(1);
          gaps += blanks().length;
        }
        const row = visible().find((row) => row.index === ${index});
        const along = list.horizontal ? list.clientWidth : list.clientHeight;
        return { start: Math.round(row.start), expected: Math.round(${position} * (along - (row.end - row.start))), gaps };
      })()`);
        /*
         * Offsets are whole pixels. A centered row whose free space is odd lands half a
         * pixel off, rounded either way.
         */
        assert.ok(
          Math.abs(landed.start - landed.expected) <= 1 && landed.gaps === 0,
          `${layout} ${index} ${JSON.stringify(landed)}`
        );
      }
    }
  }
);

browserTest(
  'a cross size change keeps the first row and an inverted end',
  async (tab) => {
    await makeList(tab, '{}', 'middle');
    const top = await tab.evaluate<{ key: string; start: number }>(
      'visible()[0]'
    );
    const resized = await tab.evaluate(`(async () => {
    list.style.width = '300px';
    await frames(3);
    const row = visible()[0];
    return { key: row.key, start: row.start };
  })()`);
    assert.deepEqual(resized, { key: top.key, start: top.start });
    await makeList(tab, '{ inverted: true }', 'end');
    const ends = await tab.evaluate(`(async () => {
    const ends = [];
    for (const width of ['300px', '500px', '400px']) {
      list.style.width = width;
      await frames(3);
      ends.push(atEnd());
    }
    return ends;
  })()`);
    assert.deepEqual(ends, [true, true, true]);
  }
);

browserTest('a list moved in the page keeps its row', async (tab) => {
  for (const layout of ['{}', '{ inverted: true }', '{ horizontal: true }']) {
    await makeList(tab, layout, 'middle');
    const moved = await tab.evaluate<{
      before: number;
      after: number;
    }>(`(async () => {
      const held = anchor();
      const parent = list.parentNode;
      list.remove();
      await frames(1);
      parent.append(list);
      await frames(3);
      return { before: held.start, after: positionOf(held.key) };
    })()`);
    assert.equal(moved.after, moved.before, layout);
  }
});

browserTest('the visible range is the rows on screen', async (tab) => {
  await makeList(tab, '{}', 'middle');
  const ranges = await tab.evaluate<{ range: object; rows: object }>(`(() => {
    const rows = visible();
    return { range: list.visibleRange, rows: { low: rows[0].index, high: rows[rows.length - 1].index } };
  })()`);
  assert.deepEqual(ranges.range, ranges.rows);
});

browserTest(
  'reached callbacks fire once per approach and load without jumps',
  async (tab) => {
    await makeList(tab, '{ count: 300 }');
    const counts = await tab.evaluate<number[]>(`(async () => {
    const counts = [list.events.start, list.events.end];
    await scrollRun(Array(60).fill(600));
    counts.push(list.events.start, list.events.end);
    await scrollRun(Array(60).fill(-600));
    counts.push(list.events.start, list.events.end);
    return counts;
  })()`);
    assert.deepEqual(counts, [1, 0, 1, 1, 2, 1]);
    for (const [layout, setup, delta] of [
      [
        '{ count: 100 }',
        'list.delegate.didReachEnd = () => list.appendRows(50)',
        150,
      ],
      [
        '{ count: 100, inverted: true }',
        'list.delegate.didReachStart = () => list.prependRows(50)',
        -150,
      ],
    ] as const) {
      await makeList(tab, layout);
      const result = await tab.evaluate<{
        issues: unknown[];
        count: number;
      }>(`(async () => {
      ${setup};
      const issues = await scrollRun(Array(100).fill(${delta}));
      return { issues, count: list.itemCount };
    })()`);
      assert.deepEqual(result.issues, [], layout);
      assert.ok(result.count > 100, layout);
    }
  }
);

browserTest('prefetch hears items ahead of the mounted rows', async (tab) => {
  await makeList(tab, '{ count: 500, prefetch: true }');
  const prefetch = await tab.evaluate<{
    first: number[];
    mounted: number;
  }>(`(async () => {
    const first = list.events.prefetched.slice();
    const mounted = Math.max(...[...list.mounted.values()].filter((cell) => cell.mountGeneration > 0).map((cell) => cell.index));
    await scrollRun(Array(40).fill(150));
    return { first, mounted, total: list.events.prefetched.length };
  })()`);
  assert.ok(prefetch.first.length > 0);
  assert.ok(prefetch.first.every((index) => index > prefetch.mounted));
});

browserTest(
  'cells read as items of the list unless the list has another role',
  async (tab) => {
    const roles = `(async () => {
      await frames(3);
      const cell = [...list.mounted.values()].find((at) => at.mountGeneration > 0);
      return {
        list: list.getAttribute('role'),
        cell: cell.view.getAttribute('role'),
        position: cell.view.getAttribute('aria-posinset'),
        size: cell.view.getAttribute('aria-setsize'),
        index: cell.index,
      };
    })()`;
    await makeList(tab, '{ count: 50 }');
    const list = await tab.evaluate<Record<string, unknown>>(roles);
    assert.deepEqual(list, {
      list: 'list',
      cell: 'listitem',
      position: String((list.index as number) + 1),
      size: '50',
      index: list.index,
    });
    await makeList(tab, "{ count: 50, role: 'rowgroup' }");
    const grid = await tab.evaluate<Record<string, unknown>>(roles);
    assert.deepEqual(grid, {
      list: 'rowgroup',
      cell: null,
      position: null,
      size: null,
      index: grid.index,
    });
  }
);

/*
 * Twelve items in four columns. The first and the last are tall and overlap every viewport,
 * which keeps the ends of the mount plan while the rows between them come and go.
 */
browserTest(
  'a grid mounts a row that comes into view between the same ends',
  async (tab) => {
    const shown = await tab.evaluate<{
      first: number[];
      later: number[];
      back: number[];
      blanks: unknown[];
    }>(`(async () => {
      const tall = new Map([[0, 2400], [2, 360], [5, 2280], [6, 2040], [11, 2280]]);
      window.sizes = new Map(Array.from({ length: 12 }, (_, id) => [id, tall.get(id) ?? 120]));
      make({ count: 12, columns: 4, sized: true });
      await frames(3);
      const indices = () => visible().map((row) => row.index);
      const first = indices();
      list.scrollTop = 2040;
      await frames(3);
      const later = indices();
      const gaps = blanks();
      list.scrollTop = 120;
      await frames(3);
      const back = indices();
      window.sizes = new Map();
      return { first, later, back, blanks: gaps };
    })()`);
    assert.ok(shown.first.includes(2) && !shown.first.includes(9));
    for (const index of [4, 9, 10]) {
      assert.ok(shown.later.includes(index), `item ${index} is mounted`);
    }
    assert.ok(!shown.later.includes(2));
    assert.deepEqual(shown.blanks, []);
    assert.ok(shown.back.includes(2) && !shown.back.includes(9));
  }
);

browserTest(
  'an item inserted by its own call or in a batch is reloaded once its content changes',
  async (tab) => {
    await makeList(tab, '{ count: 20 }');
    const result = await tab.evaluate<{
      own: number[];
      kept: number[];
      batch: number[];
    }>(`(async () => {
      const versions = new Map();
      list.dataSource.contentVersionForItem = (_, index) =>
        versions.get(list.state.ids[index]) ?? 0;
      list.reloadData();
      list.appendRows(1);
      versions.set(list.state.ids[20], 1);
      const own = list.applyChanges().reloadedIndices;
      // Row 5 changed and nobody told the list yet. An insert keeps that for the next diff.
      versions.set(list.state.ids[5], 1);
      list.prependRows(1);
      const kept = list.applyChanges().reloadedIndices;
      list.performBatchUpdates(() => {
        list.removeFirstRows(1);
        list.appendRows(2);
      });
      versions.set(list.state.ids[21], 1);
      const batch = list.applyChanges().reloadedIndices;
      return { own, kept, batch };
    })()`);
    assert.deepEqual(result.own, [20]);
    assert.deepEqual(result.kept, [6]);
    assert.deepEqual(result.batch, [21]);
  }
);

browserTest(
  'a batch lands as one change and applyChanges reloads changed versions',
  async (tab) => {
    await makeList(tab, '{ count: 20 }');
    const result = await tab.evaluate<{
      keys: string[];
      reloaded: number[];
      again: number[];
    }>(`(async () => {
      const ids = list.state.ids;
      list.performBatchUpdates(() => {
        // Delete row 1, move row 3 to the front, insert a new row at the end.
        list.state.ids = [ids[3], ids[0], ids[2], ...ids.slice(4), 500];
        list.deleteItems([1]);
        list.moveItem(3, 0);
        list.insertItems([19]);
      });
      await frames(2);
      const keys = list.keys.slice(0, 4).concat(list.keys.slice(-1));
      const versions = new Map(list.state.ids.map((id) => [id, 'v1']));
      list.dataSource.contentVersionForItem = (_, index) =>
        versions.get(list.state.ids[index]);
      list.reloadData();
      versions.set(list.state.ids[2], 'v2');
      versions.set(list.state.ids[5], 7);
      const reloaded = list.applyChanges().reloadedIndices;
      const again = list.applyChanges().reloadedIndices;
      return { keys, reloaded, again };
    })()`);
    assert.deepEqual(result.keys, ['k3', 'k0', 'k2', 'k4', 'k500']);
    assert.deepEqual(result.reloaded, [2, 5]);
    assert.deepEqual(result.again, []);
  }
);

/*
 * A header of 50 and a footer of 40 on window.list, and where each shows: its start and end on
 * screen along the scroll axis, relative to the viewport start.
 */
const TEMPLATES = `
  const template = (size) => {
    const view = document.createElement('div');
    // The content gives the size the list measures.
    view.style.minHeight = size + 'px';
    return view;
  };
  const place = (view) => {
    const box = list.getBoundingClientRect();
    const rect = view.getBoundingClientRect();
    return [Math.round(rect.top - box.top), Math.round(rect.bottom - box.top)];
  };
`;

browserTest(
  'a sticky header stays at the viewport start and a sticky footer at its end',
  async (tab) => {
    await makeList(tab, '{ sized: true }');
    const placed = await tab.evaluate<Record<string, number[]>>(`(async () => {
      ${TEMPLATES}
      const header = template(50);
      const footer = template(40);
      list.headerView = header;
      list.footerView = footer;
      list.stickyHeader = true;
      list.stickyFooter = true;
      await frames(3);
      const placed = { start: [...place(header), ...place(footer)] };
      list.scrollTop = 3000;
      await frames(3);
      placed.middle = [...place(header), ...place(footer)];
      placed.rows = blanks();
      list.scrollToEnd();
      await frames(4);
      placed.end = [...place(header), ...place(footer)];
      placed.atEnd = [list.scrollTop + list.clientHeight - list.scrollHeight];
      list.stickyHeader = false;
      list.stickyFooter = false;
      list.scrollTop = 3000;
      await frames(3);
      placed.plain = [place(header)[0], header.parentElement === footer.parentElement ? 1 : 0];
      return placed;
    })()`);
    assert.deepEqual(placed.start, [0, 50, 560, 600]);
    assert.deepEqual(placed.middle, [0, 50, 560, 600]);
    assert.deepEqual(placed.rows, []);
    assert.deepEqual(placed.end, [0, 50, 560, 600]);
    assert.deepEqual(placed.atEnd, [0]);
    assert.deepEqual(placed.plain, [-3000, 1]);
  }
);

browserTest(
  'the footer pushes a sticky header off and a sticky footer rests at the end of a short list',
  async (tab) => {
    await makeList(tab, '{ sized: true }');
    const pushed = await tab.evaluate<number[]>(`(async () => {
      ${TEMPLATES}
      const header = template(50);
      const footer = template(40);
      list.headerView = header;
      list.footerView = footer;
      list.stickyHeader = true;
      // A viewport shorter than the two.
      list.style.height = '70px';
      await frames(3);
      list.scrollToEnd();
      await frames(4);
      return [...place(header), ...place(footer)];
    })()`);
    assert.deepEqual(pushed, [-20, 30, 30, 70]);

    await makeList(tab, '{ sized: true, count: 3 }');
    const short = await tab.evaluate<number[]>(`(async () => {
      ${TEMPLATES}
      const footer = template(40);
      list.footerView = footer;
      list.stickyFooter = true;
      await frames(3);
      return [...place(footer), list.scrollHeight - list.clientHeight, list.contentSize];
    })()`);
    assert.equal(short[1], 600);
    assert.equal(short[0], 560);
    assert.equal(short[2], 0);
    assert.ok(short[3]! < 600);
  }
);

browserTest(
  'the empty view shows after the header while the list has no items',
  async (tab) => {
    await makeList(tab, '{ sized: true, count: 0 }');
    const shown = await tab.evaluate<Record<string, unknown[]>>(`(async () => {
      ${TEMPLATES}
      const header = template(50);
      const footer = template(40);
      const empty = template(200);
      list.headerView = header;
      list.footerView = footer;
      list.emptyView = empty;
      await frames(3);
      const shown = {
        empty: [getComputedStyle(empty).visibility, ...place(empty), ...place(footer), list.contentSize],
      };
      list.appendRows(2);
      await frames(3);
      shown.rows = [getComputedStyle(empty).visibility, place(footer)[0] - place(header)[1], list.itemCount];
      list.removeFirstRows(2);
      await frames(3);
      shown.again = [getComputedStyle(empty).visibility, ...place(footer)];
      list.emptyView = null;
      await frames(3);
      shown.none = [empty.isConnected, ...place(footer), list.contentSize];
      return shown;
    })()`);
    assert.deepEqual(shown.empty, ['visible', 50, 250, 250, 290, 290]);
    assert.equal(shown.rows![0], 'hidden');
    assert.ok((shown.rows![1] as number) > 0 && shown.rows![1] !== 200);
    assert.deepEqual(shown.again, ['visible', 250, 290]);
    assert.deepEqual(shown.none, [false, 50, 90, 90]);
  }
);

browserTest(
  'an inverted list tells when it leaves its end and is back',
  async (tab) => {
    await makeList(tab, '{ inverted: true, sized: true }', 'end');
    const told = await tab.evaluate<Record<string, unknown>>(`(async () => {
      const told = { opened: [list.atEnd, ...list.events.atEnd] };
      list.appendRows(3);
      await frames(3);
      told.appended = [list.atEnd, ...list.events.atEnd];
      list.scrollTop -= 1500;
      await frames(3);
      told.away = [list.atEnd, ...list.events.atEnd];
      list.scrollToEnd();
      await frames(4);
      told.back = [list.atEnd, ...list.events.atEnd];
      return told;
    })()`);
    assert.deepEqual(told.opened, [true]);
    assert.deepEqual(told.appended, [true]);
    assert.deepEqual(told.away, [false, false]);
    assert.deepEqual(told.back, [true, false, true]);
  }
);

browserTest('a non anchor item never holds the visible items', async (tab) => {
  for (const nonAnchor of [false, true]) {
    await makeList(tab, '{ sized: true }', 'middle');
    const moved = await tab.evaluate<number[]>(`(async () => {
      const [first, next] = visible();
      const id = Number(first.key.slice(1));
      if (${nonAnchor}) list.nonAnchorKeys = new Set([first.key]);
      await frames(2);
      // The item at the viewport start grows.
      sizes.set(id, 400);
      list.reloadItems([list.keys.indexOf(first.key)]);
      await frames(3);
      sizes.delete(id);
      return [positionOf(first.key) - first.start, positionOf(next.key) - next.start];
    })()`);
    // One of the two stays where it was: the first item, or the one after it.
    assert.equal(moved[nonAnchor ? 1 : 0], 0, `nonAnchor ${nonAnchor}`);
    assert.notEqual(moved[nonAnchor ? 0 : 1], 0, `nonAnchor ${nonAnchor}`);
  }
});

test(
  'actual web routes mount their cells',
  { skip: CHROME ? WEB_EXAMPLE_SKIP : 'no Chrome found' },
  async () => {
    for (const route of [
      'Chat',
      'Masonry',
      'Horizontal',
      'Feed',
      'Poll',
      'Sticky',
    ]) {
      const tab = await Page.open(
        browserPort,
        `http://127.0.0.1:${serverPort}/web/?SLRoute=${route}&SLCount=20`,
        'Boolean(window.list)'
      );
      try {
        const result = await tab.evaluate<{
          mounted: number;
        }>(`(async () => {
        await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
        return {
          mounted: window.list.visibleCells.length,
        };
      })()`);
        assert.ok(result.mounted > 0, `${route} mounted its actual cells`);
        assert.deepEqual(tab.errors, [], route);
      } finally {
        await tab.send('Page.navigate', { url: 'about:blank' });
        tab.close();
      }
    }
  }
);
