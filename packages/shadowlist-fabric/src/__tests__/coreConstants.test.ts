import { describe, expect, it } from '@jest/globals';
import { readFileSync } from 'fs';
import { join } from 'path';
import {
  SHADOWLIST_OVERSCAN,
  SHADOWLIST_OVERSCAN_LEADING,
  SNAP_ALIGNMENT,
} from '../virtualizer/helpers';
import {
  SECTIONS_FOOTER,
  SECTIONS_HEADER,
  SECTIONS_QUALIFIED,
  SECTIONS_STICKY,
  TREE_EXPANDED,
  TREE_HAS_CHILDREN,
} from '../virtualizer/nativeStore';
import { HINT_WINDOW } from '../virtualizer/rowSource';
import {
  FOOTER_KEY_PREFIX,
  HEADER_KEY_PREFIX,
  ITEM_KEY_PREFIX,
  ITEM_KEY_SEPARATOR,
} from '../virtualizer/sectionKeys';
import { MAX_FOLLOWED_APPEND } from '../virtualizer/useMountedRange';

const CORE_DIR = join(__dirname, '../../../shadowlist-core');
const SPEC_DIR = join(
  __dirname,
  '../../cpp/react/renderer/components/ShadowListViewSpec'
);

/*
 * The number the core constant name is set to in a header of packages/shadowlist-core, written
 * in decimal or in hex.
 */
function coreConstant(header: string, name: string): number {
  const source = readFileSync(join(CORE_DIR, header), 'utf8');
  const match = new RegExp(
    `constexpr [\\w:]+ ${name} = (0x[\\dA-Fa-f]+|[\\d.]+);`
  ).exec(source);
  if (!match) {
    throw new Error(`${name} is not in ${header}`);
  }
  return Number(match[1]);
}

/*
 * The text the core constant name is set to: one character, or string literals side by side,
 * with their \x escapes read.
 */
function coreText(header: string, name: string): string {
  const source = readFileSync(join(CORE_DIR, header), 'utf8');
  const match = new RegExp(
    `constexpr (?:const char\\*|char) ${name} = ([^;]+);`
  ).exec(source);
  if (!match) {
    throw new Error(`${name} is not in ${header}`);
  }
  const literals = match[1]!.match(/"[^"]*"|'[^']*'/g) ?? [];
  return literals
    .map((literal) =>
      literal
        .slice(1, -1)
        .replace(/\\x([\dA-Fa-f]{2})/g, (_, hex: string) =>
          String.fromCharCode(parseInt(hex, 16))
        )
    )
    .join('');
}

/*
 * The value of an enum class member in a core header.
 */
function coreEnumValue(header: string, type: string, member: string): number {
  const source = readFileSync(join(CORE_DIR, header), 'utf8');
  const body = new RegExp(`enum class ${type} \\{([^}]+)\\}`).exec(source);
  const match = new RegExp(`\\b${member} = (\\d+),`).exec(body?.[1] ?? '');
  if (!match) {
    throw new Error(`${type}::${member} is not in ${header}`);
  }
  return Number(match[1]);
}

/*
 * A source file of this package, for a number its code writes in place.
 */
function packageSource(path: string): string {
  return readFileSync(join(__dirname, '..', path), 'utf8');
}

/*
 * All members of a bridge enum, including implicit successive values. An expression needs an
 * explicit decoder here rather than silently dropping a declaration from the parity check.
 */
function bridgeEnum(source: string, name: string): Record<string, number> {
  const body = new RegExp(`enum ${name} : int \\{([^}]+)\\}`).exec(source);
  if (!body) throw new Error(`Missing bridge enum ${name}`);
  const values: Record<string, number> = {};
  let next = 0;
  for (const declaration of body[1]!.split(',')) {
    const member = declaration.trim();
    if (!member) continue;
    const match = /^([A-Z_]+)(?:\s*=\s*(0x[\dA-Fa-f]+|\d+))?$/.exec(member);
    if (!match) throw new Error(`Unparsed ${name} member: ${member}`);
    if (match[2]) next = Number(match[2]);
    if (match[1]! in values) throw new Error(`Duplicate ${name}.${match[1]}`);
    values[match[1]!] = next++;
  }
  return values;
}

function bridgeConstants(
  source: string,
  prefix: string
): Record<string, number> {
  const values: Record<string, number> = {};
  const declarations = new RegExp(
    `(?:const\\s+val|static\\s+readonly)\\s+(${prefix}[A-Z_]+)\\s*=\\s*([^;\\n]+)`,
    'g'
  );
  for (const match of source.matchAll(declarations)) {
    const value = match[2]!.trim();
    if (!/^(0x[\dA-Fa-f]+|\d+)$/.test(value)) {
      throw new Error(`Unparsed bridge constant ${match[1]}: ${value}`);
    }
    if (match[1]! in values) throw new Error(`Duplicate ${match[1]}`);
    values[match[1]!] = Number(value);
  }
  return values;
}

describe('kit bridge declarations', () => {
  it('maps every shared slot and commit flag exactly across C++, Kotlin and TypeScript', () => {
    const core = readFileSync(join(CORE_DIR, 'host/KitBridge.hpp'), 'utf8');
    const android = readFileSync(
      join(
        CORE_DIR,
        '../shadowlist-android/ShadowListKit/src/main/java/com/shadowlist/kit/ShadowListKitCore.kt'
      ),
      'utf8'
    );
    const androidState = readFileSync(
      join(
        CORE_DIR,
        '../shadowlist-android/ShadowListKit/src/main/java/com/shadowlist/kit/ShadowListKitListState.kt'
      ),
      'utf8'
    );
    const wasm = readFileSync(
      join(CORE_DIR, '../shadowlist-wasm/src/ShadowListKitCore.ts'),
      'utf8'
    );
    const groups = [
      ['PassSlot', 'PASS_'],
      ['MountSlot', 'MOUNT_'],
      ['StickySlot', 'STICKY_'],
      ['CommitFlag', 'COMMIT_'],
    ];
    expect(
      Array.from(
        core.matchAll(/^enum (\w+) : int /gm),
        (match) => match[1]
      ).sort()
    ).toEqual(
      [...groups.map(([name]) => name), 'ViewabilityConfigSlot'].sort()
    );
    for (const [name, prefix] of groups) {
      const expected = bridgeEnum(core, name!);
      expect(bridgeConstants(android + '\n' + androidState, prefix!)).toEqual(
        expected
      );
      expect(bridgeConstants(wasm, prefix!)).toEqual(expected);
    }
    expect(bridgeConstants(android, 'VIEWABILITY_')).toEqual(
      bridgeEnum(core, 'ViewabilityConfigSlot')
    );
    // The web kit has no viewability config bridge. Adding one must extend this mapping.
    expect(bridgeConstants(wasm, 'VIEWABILITY_')).toEqual({});
  });
});

describe('core constants', () => {
  it('match the mounted range numbers the JS range math uses', () => {
    expect(SHADOWLIST_OVERSCAN).toBe(
      coreConstant('host/MountedRange.hpp', 'DEFAULT_OVERSCAN_ROWS')
    );
    expect(SHADOWLIST_OVERSCAN_LEADING).toBe(
      coreConstant('host/MountedRange.hpp', 'DEFAULT_OVERSCAN_ROWS_LEADING')
    );
    expect(MAX_FOLLOWED_APPEND).toBe(
      coreConstant('host/MountedRange.hpp', 'MAX_FOLLOWED_APPEND')
    );
  });

  it('match the window a hinted key lookup searches', () => {
    expect(HINT_WINDOW).toBe(
      coreConstant('host/RowKeyStore.hpp', 'HINT_WINDOW')
    );
  });

  it('match the row keys of a sectioned list', () => {
    expect(HEADER_KEY_PREFIX).toBe(
      coreText('host/ListSections.hpp', 'HEADER_KEY_PREFIX')
    );
    expect(FOOTER_KEY_PREFIX).toBe(
      coreText('host/ListSections.hpp', 'FOOTER_KEY_PREFIX')
    );
    expect(ITEM_KEY_PREFIX).toBe(
      coreText('host/ListSections.hpp', 'ITEM_KEY_PREFIX')
    );
    expect(ITEM_KEY_SEPARATOR).toBe(
      coreText('host/ListSections.hpp', 'ITEM_KEY_SEPARATOR')
    );
  });

  it('match the tree row flags', () => {
    expect(TREE_HAS_CHILDREN).toBe(
      coreConstant('host/TreeRows.hpp', 'TREE_ROW_HAS_CHILDREN')
    );
    expect(TREE_EXPANDED).toBe(
      coreConstant('host/TreeRows.hpp', 'TREE_ROW_EXPANDED')
    );
  });

  it('match the section flags the store documents', () => {
    const contract = readFileSync(
      join(SPEC_DIR, 'ShadowListStoreJsi.h'),
      'utf8'
    );
    const flags =
      /flags: (\d+) headers, (\d+) footers, (\d+) sticky headers, (\d+) item keys qualified by section/.exec(
        contract
      );
    expect(flags?.slice(1).map(Number)).toEqual([
      SECTIONS_HEADER,
      SECTIONS_FOOTER,
      SECTIONS_STICKY,
      SECTIONS_QUALIFIED,
    ]);
  });

  it('match the snap alignment enum', () => {
    expect(SNAP_ALIGNMENT).toEqual({
      start: coreEnumValue('Container.hpp', 'SnapAlignment', 'Start'),
      center: coreEnumValue('Container.hpp', 'SnapAlignment', 'Center'),
      end: coreEnumValue('Container.hpp', 'SnapAlignment', 'End'),
    });
  });

  it('match the backgrounds of a swipe action that sets none', () => {
    const fallback =
      /style === 'destructive' \? '#([\dA-F]{6})' : '#([\dA-F]{6})'/.exec(
        packageSource('virtualizer/useCellActions.ts')
      );
    const opaque = (hex: string | undefined) => parseInt(`FF${hex ?? ''}`, 16);
    expect(opaque(fallback?.[1])).toBe(
      coreConstant('host/SwipeReveal.hpp', 'SWIPE_ACTION_COLOR_DESTRUCTIVE')
    );
    expect(opaque(fallback?.[2])).toBe(
      coreConstant('host/SwipeReveal.hpp', 'SWIPE_ACTION_COLOR_NORMAL')
    );
  });

  it('match the width of the section index', () => {
    const width = /index: \{[^}]*\bwidth: (\d+),/.exec(
      packageSource('virtualizer/SectionIndex.tsx')
    );
    expect(Number(width?.[1])).toBe(
      coreConstant('host/SectionIndex.hpp', 'SECTION_INDEX_WIDTH')
    );
  });
});
