/*
 * The list in headless Chrome, driven over the DevTools protocol through test/fixture.html.
 * Skipped when no Chrome is found. CHROME_PATH picks the binary. Run ./build.sh first.
 */
import assert from 'node:assert/strict';
import { spawn, type ChildProcess } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { createServer, type Server } from 'node:http';
import { tmpdir } from 'node:os';
import { extname, join, normalize } from 'node:path';
import { after, before, test } from 'node:test';

const ROOT = new URL('..', import.meta.url).pathname;

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

  static async open(debugPort: number, url: string): Promise<Page> {
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
      if (await opened.evaluate('window.fixtureReady === true')) return opened;
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
    const path = normalize(
      join(ROOT, decodeURIComponent(new URL(request.url!, 'http://x').pathname))
    );
    if (!path.startsWith(ROOT) || !existsSync(path)) {
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
  page = await Page.open(
    debugPort,
    `http://127.0.0.1:${port}/test/fixture.html`
  );
});

after(async () => {
  page?.close();
  chrome?.removeAllListeners('exit');
  // Chrome writes its profile until it exits. Remove the profile after that.
  if (chrome && chrome.exitCode === null) {
    const exited = new Promise((resolve) => chrome!.once('exit', resolve));
    chrome.kill();
    await Promise.race([
      exited,
      new Promise((resolve) => setTimeout(resolve, 5000).unref()),
    ]);
  }
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
