import { Text } from 'react-native';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import { test } from 'node:test';
import * as tty from '../src/index';
import { renderApp } from './helpers';

test("offers every value shadowlist-fabric's index names", () => {
  const source = readFileSync(
    path.resolve(
      process.env.SHADOWLIST_TTY_ADDON!,
      '../../../shadowlist-fabric/src/index.tsx'
    ),
    'utf8'
  );
  const named = [...source.matchAll(/^export \{([^}]*)\}/gm)]
    .flatMap((match) => match[1]!.split(','))
    .map((name) => name.trim().split(' as ').pop()!)
    .filter((name) => name !== '');
  assert.ok(named.includes('DraggableList'));
  assert.deepEqual(
    named.filter((name) => !(name in tty)),
    []
  );
  assert.ok('ShadowListBase' in tty);
  assert.ok('initialMountedRange' in tty);
});

test('ItemActionsView and KeyboardView draw what they hold', async () => {
  const host = await renderApp(
    <tty.KeyboardView>
      <tty.ItemActionsView>
        <Text>inside</Text>
      </tty.ItemActionsView>
    </tty.KeyboardView>
  );
  assert.match(host.snapshot(), /inside/);
});

test('DraggableList is a ShadowList', async () => {
  const host = await renderApp(
    <tty.DraggableList
      data={['one', 'two']}
      keyExtractor={(item) => item}
      renderItem={({ item }) => <Text>{item}</Text>}
    />,
    { columns: 10, rows: 3 }
  );
  assert.match(host.snapshot(), /one\s+two/);
});

test('useKeyboardAnimation stays at 0', async () => {
  let height = -1;
  const Probe = () => {
    const animation = tty.useKeyboardAnimation();
    height = (
      animation.height as unknown as { __getValue(): number }
    ).__getValue();
    return null;
  };
  await renderApp(<Probe />);
  assert.equal(height, 0);
});
