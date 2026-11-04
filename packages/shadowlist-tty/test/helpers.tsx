import type { ReactNode } from 'react';
import { AppRegistry } from 'react-native';
import { after } from 'node:test';
import { createTestHost, type TestHost } from 'react-native-tty/host';
import { setShadowListAddonPath } from '../src';

setShadowListAddonPath(
  new URL('../build/shadowlist_tty.node', import.meta.url).pathname
);

/*
 * AppRegistry as the platform has it. tsc reads react-native's own types, which know no host.
 */
const registry = AppRegistry as unknown as {
  registerComponent(key: string, provider: () => () => ReactNode): void;
  runApplication(key: string, options: { host: TestHost }): { stop(): void };
};

let nextKey = 1;
const running: { stop(): void }[] = [];

after(() => {
  for (const app of running) {
    app.stop();
  }
});

/*
 * Runs an element in a test host and waits for it to be on the screen.
 */
export async function renderApp(
  element: ReactNode,
  size: { columns?: number; rows?: number } = {}
): Promise<TestHost> {
  const key = `list-${nextKey++}`;
  registry.registerComponent(key, () => () => element);
  const host = createTestHost(size);
  running.push(registry.runApplication(key, { host }));
  await host.settle();
  return host;
}

export function wheelEvent(deltaY: number, deltaX = 0): object {
  return {
    type: 'wheel',
    x: 1,
    y: 1,
    deltaX,
    deltaY,
    shiftKey: false,
    ctrlKey: false,
    altKey: false,
    metaKey: false,
  };
}

export function lines(host: TestHost): string[] {
  return host.snapshot().split('\n');
}
