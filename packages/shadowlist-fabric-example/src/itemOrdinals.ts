import { useCallback, useMemo, useRef } from 'react';

/*
 * Stable position labels for rows, like first item or second item.
 * A row's number is given once, when it first appears, and kept by id rather than by index.
 * Loading chat history shifts every index, and labels based on the index would renumber and
 * re-render every mounted row at once.
 * Rows that arrive before the numbered ones count separately, away from the start, so each
 * prepended page keeps its own sequence. Rows appended after the last numbered one continue
 * the forward count.
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
 * Positions are spelled out in full, like five hundred thirty ninth item. Intl can only pick
 * the suffix category and has no spell-out mode, so the few English rules live here behind a cache.
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

/*
 * The words for a number, for example 539 gives five hundred thirty nine.
 */
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

// Turns twenty into twentieth, nine into ninth and hundred into hundredth.
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
  if (word.endsWith('y')) return `${word.slice(0, -1)}ieth`;
  return `${word}th`;
}

const labelCache = new Map<string, string>();

/**
 * The spelled out label for a position, like first item or five hundred thirty ninth item.
 * Rows numbered away from the start read prepended: first item.
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
 * Numbers each row of data once, in arrival order, and returns a stable labelOf. Safe to call
 * on every render, since only new rows are numbered and a prepend costs one pass over its page.
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
   * Numbered during render, not in an effect, because this commit's rows read their labels as
   * they render. An effect would leave the first frame of a new page unlabelled.
   */
  useMemo(() => {
    const registry = registryRef.current;
    if (data.length === 0) return;

    const headIndex =
      registry.headId === null
        ? -1
        : data.findIndex((item) => item.id === registry.headId);

    if (headIndex === -1) {
      // Nothing is numbered yet, or the numbered rows are gone, so this is the baseline page.
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
       * Rows before the old head are a prepended page. Number them from the row nearest the head
       * outwards, so each older page keeps counting where the last one stopped.
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
   * Stable for the life of the screen. The row renderer reads it, and a new identity would
   * rebuild every mounted row.
   */
  const labelOf = useCallback((id: string) => {
    const number = registryRef.current.numbers.get(id);
    return number === undefined
      ? ''
      : formatOrdinalLabel(number.value, number.prepended);
  }, []);

  return useMemo(() => ({ labelOf }), [labelOf]);
}
