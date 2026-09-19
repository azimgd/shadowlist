import type { DeepPartial } from './DeepPartial';
import type { Theme } from './Theme';

type PlainObject = Record<string, unknown>;

const isPlainObject = (value: unknown): value is PlainObject =>
  typeof value === 'object' && value !== null && !Array.isArray(value);

function mergeDeep(base: PlainObject, overrides: PlainObject): PlainObject {
  const result: PlainObject = { ...base };
  for (const key of Object.keys(overrides)) {
    const override = overrides[key];
    if (override === undefined) {
      continue;
    }
    const current = base[key];
    result[key] =
      isPlainObject(current) && isPlainObject(override)
        ? mergeDeep(current, override)
        : override;
  }
  return result;
}

export function createTheme(base: Theme, overrides: DeepPartial<Theme>): Theme {
  return mergeDeep(
    base as unknown as PlainObject,
    overrides as PlainObject
  ) as unknown as Theme;
}
