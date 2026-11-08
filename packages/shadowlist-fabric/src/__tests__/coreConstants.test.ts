import { describe, expect, it } from '@jest/globals';
import { readFileSync } from 'fs';
import { join } from 'path';
import {
  SHADOWLIST_OVERSCAN,
  SHADOWLIST_OVERSCAN_LEADING,
} from '../virtualizer/helpers';
import { MAX_FOLLOWED_APPEND } from '../virtualizer/useMountedRange';

/*
 * The core constant name's value in a header of packages/shadowlist-core/host.
 */
function coreConstant(header: string, name: string): number {
  const source = readFileSync(
    join(__dirname, '../../../shadowlist-core/host', header),
    'utf8'
  );
  const match = new RegExp(`constexpr [\\w:]+ ${name} = ([\\d.]+);`).exec(
    source
  );
  if (!match) {
    throw new Error(`${name} is not in ${header}`);
  }
  return Number(match[1]);
}

describe('core constants', () => {
  it('match the mounted range numbers the JS range math uses', () => {
    expect(SHADOWLIST_OVERSCAN).toBe(
      coreConstant('MountedRange.hpp', 'DEFAULT_OVERSCAN_ROWS')
    );
    expect(SHADOWLIST_OVERSCAN_LEADING).toBe(
      coreConstant('MountedRange.hpp', 'DEFAULT_OVERSCAN_ROWS_LEADING')
    );
    expect(MAX_FOLLOWED_APPEND).toBe(
      coreConstant('MountedRange.hpp', 'MAX_FOLLOWED_APPEND')
    );
  });
});
