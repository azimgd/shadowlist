import { useCallback, useMemo, useRef } from 'react';

/*
 * Stable position labels for list rows ("first item", "second item", ...).
 *
 * A row's number is assigned ONCE, when the row first appears, and kept in a registry keyed
 * by id. It deliberately does not follow the row's array index: history loaded into a chat
 * shifts every index by a page, and a label derived from the index would change on every
 * mounted row at once -- a full window of re-renders for text nobody expected to move, and
 * the numbers under the reader's eyes would renumber themselves as older messages arrive.
 *
 * Rows that arrive BEFORE the ones already numbered are counted separately, away from the
 * start, so the pages keep their own sequence:
 *
 *   loaded:            | 1, 2, 3, 4, 5
 *   after a prepend:    5, 4, 3, 2, 1 | 1, 2, 3, 4, 5
 *   after another:     10, 9, 8, 7, 6, 5, 4, 3, 2, 1 | 1, 2, 3, 4, 5
 *
 * Rows appended after the last numbered one continue the forward count (6, 7, 8, ...).
 */

interface ItemNumber {
  value: number;
  prepended: boolean;
}

interface OrdinalRegistry {
  numbers: Map<string, ItemNumber>;
  headId: string | null;
  tailId: string | null;
  forwardCount: number;
  prependedCount: number;
}

export interface ItemOrdinals {
  labelOf: (id: string) => string;
}

/*
 * Positions are spelled out in full ("five hundred thirty ninth item").
 *
 * ECMA-402 cannot do this: `Intl.PluralRules` with `type: 'ordinal'` only classifies a number
 * into the categories behind the SUFFIXES (one -> "st", two -> "nd", few -> "rd", other ->
 * "th"), and `Intl.NumberFormat` has no spell-out mode -- ICU's `spellout-ordinal` ruleset is
 * not exposed to JavaScript. So the English rules live here: they are small, and they run
 * once per position, behind a cache.
 */
const ONES = [
  'zero',
  'one',
  'two',
  'three',
  'four',
  'five',
  'six',
  'seven',
  'eight',
  'nine',
  'ten',
  'eleven',
  'twelve',
  'thirteen',
  'fourteen',
  'fifteen',
  'sixteen',
  'seventeen',
  'eighteen',
  'nineteen',
];

const TENS = [
  '',
  '',
  'twenty',
  'thirty',
  'forty',
  'fifty',
  'sixty',
  'seventy',
  'eighty',
  'ninety',
];

const SCALES: ReadonlyArray<readonly [number, string]> = [
  [1000000000, 'billion'],
  [1000000, 'million'],
  [1000, 'thousand'],
  [100, 'hundred'],
];

// The words for a cardinal number, e.g. 539 -> ['five', 'hundred', 'thirty', 'nine'].
function cardinalWords(value: number): string[] {
  if (value < 20) return [ONES[value] ?? String(value)];

  for (const [scale, name] of SCALES) {
    if (value >= scale) {
      const words = [...cardinalWords(Math.floor(value / scale)), name];
      const remainder = value % scale;
      return remainder === 0 ? words : [...words, ...cardinalWords(remainder)];
    }
  }

  const tens = TENS[Math.floor(value / 10)] ?? '';
  const ones = value % 10;
  return ones === 0 ? [tens] : [tens, ONES[ones]!];
}

// "twenty" -> "twentieth", "nine" -> "ninth", "hundred" -> "hundredth".
const ORDINAL_IRREGULARS: Record<string, string> = {
  one: 'first',
  two: 'second',
  three: 'third',
  five: 'fifth',
  eight: 'eighth',
  nine: 'ninth',
  twelve: 'twelfth',
};

function toOrdinalWord(word: string): string {
  const irregular = ORDINAL_IRREGULARS[word];
  if (irregular !== undefined) return irregular;
  // twenty -> twentieth, forty -> fortieth, ...
  if (word.endsWith('y')) return `${word.slice(0, -1)}ieth`;
  return `${word}th`;
}

const labelCache = new Map<string, string>();

/**
 * The label for a position, spelled out: `first item`, `twenty first item`,
 * `five hundred thirty ninth item`. Rows numbered away from the start read
 * `prepended: first item`.
 */
export function formatOrdinalLabel(value: number, prepended = false): string {
  const key = `${prepended ? 'p' : 'f'}${value}`;
  const cached = labelCache.get(key);
  if (cached !== undefined) return cached;

  const words = cardinalWords(value);
  const ordinal = [
    ...words.slice(0, -1),
    toOrdinalWord(words[words.length - 1]!),
  ].join(' ');
  const label = prepended ? `prepended: ${ordinal} item` : `${ordinal} item`;
  labelCache.set(key, label);
  return label;
}

/**
 * Numbers every row of `data` once, in the order the rows arrive, and returns a stable
 * `labelOf(id)`. Safe to call on every render: only rows that are not numbered yet are
 * touched, so a prepend costs one pass over the new page rather than over the list.
 */
export function useItemOrdinals<ItemT extends { id: string }>(
  data: ReadonlyArray<ItemT>
): ItemOrdinals {
  const registryRef = useRef<OrdinalRegistry>({
    numbers: new Map(),
    headId: null,
    tailId: null,
    forwardCount: 0,
    prependedCount: 0,
  });

  /*
   * Assigned during render, not in an effect: the rows of this very commit read their label
   * while they render, and an effect would leave the first frame of a new page unlabelled.
   */
  useMemo(() => {
    const registry = registryRef.current;
    if (data.length === 0) return;

    const headIndex =
      registry.headId === null
        ? -1
        : data.findIndex((item) => item.id === registry.headId);

    if (headIndex === -1) {
      // Nothing numbered yet (or the numbered rows are gone): this is the baseline page.
      registry.numbers.clear();
      registry.forwardCount = 0;
      registry.prependedCount = 0;
      data.forEach((item) => {
        registry.numbers.set(item.id, {
          value: ++registry.forwardCount,
          prepended: false,
        });
      });
    } else {
      /*
       * Rows before the old head are a prepended page. Number them from the row nearest the
       * head outwards, so the page reads 1, 2, 3 ... as it climbs away from the baseline and
       * an older page keeps counting where the previous one stopped.
       */
      for (let index = headIndex - 1; index >= 0; index--) {
        const item = data[index]!;
        if (registry.numbers.has(item.id)) continue;
        registry.numbers.set(item.id, {
          value: ++registry.prependedCount,
          prepended: true,
        });
      }
    }

    // Rows after the last numbered one continue the forward count.
    const tailIndex =
      registry.tailId === null
        ? -1
        : data.findIndex((item) => item.id === registry.tailId);
    for (let index = tailIndex + 1; index < data.length; index++) {
      const item = data[index]!;
      if (registry.numbers.has(item.id)) continue;
      registry.numbers.set(item.id, {
        value: ++registry.forwardCount,
        prepended: false,
      });
    }

    registry.headId = data[0]!.id;
    registry.tailId = data[data.length - 1]!.id;
  }, [data]);

  /*
   * Stable for the lifetime of the screen: it is read by the row renderer, and a new identity
   * there would rebuild every mounted row's content.
   */
  const labelOf = useCallback((id: string) => {
    const number = registryRef.current.numbers.get(id);
    return number === undefined
      ? ''
      : formatOrdinalLabel(number.value, number.prepended);
  }, []);

  return useMemo(() => ({ labelOf }), [labelOf]);
}
