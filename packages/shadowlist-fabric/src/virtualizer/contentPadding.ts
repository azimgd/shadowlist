import {
  StyleSheet,
  type Insets,
  type StyleProp,
  type ViewStyle,
} from 'react-native';

/*
 * Padding around the rows, along the scroll axis (leading before the first row, trailing after
 * the last) and across it.
 */
export interface ContentPadding {
  leading: number;
  trailing: number;
  crossStart: number;
  crossEnd: number;
}

function numberOr(...values: unknown[]): number {
  for (const value of values) {
    if (typeof value === 'number' && Number.isFinite(value)) return value;
  }
  return 0;
}

/*
 * The padding of contentContainerStyle plus contentInset. Only numeric padding counts. The
 * rest of contentContainerStyle has no container to apply to, since the rows are placed one by
 * one.
 */
export function contentPadding(
  style: StyleProp<ViewStyle>,
  inset: Insets | undefined,
  horizontal: boolean
): ContentPadding {
  const flat = (StyleSheet.flatten(style) ?? {}) as ViewStyle;
  const top =
    numberOr(flat.paddingTop, flat.paddingVertical, flat.padding) +
    numberOr(inset?.top);
  const bottom =
    numberOr(flat.paddingBottom, flat.paddingVertical, flat.padding) +
    numberOr(inset?.bottom);
  const left =
    numberOr(
      flat.paddingLeft,
      flat.paddingStart,
      flat.paddingHorizontal,
      flat.padding
    ) + numberOr(inset?.left);
  const right =
    numberOr(
      flat.paddingRight,
      flat.paddingEnd,
      flat.paddingHorizontal,
      flat.padding
    ) + numberOr(inset?.right);
  return horizontal
    ? { leading: left, trailing: right, crossStart: top, crossEnd: bottom }
    : { leading: top, trailing: bottom, crossStart: left, crossEnd: right };
}

/*
 * The cross axis padding of each column's rows. The outer padding sits on the first and last
 * column and gap between columns, and every column keeps the same content size. Column c of n
 * starts start + c * (gap - extra / n) into its cell, where extra is all the padding and gaps.
 * Null when there is none.
 */
export function rowPaddingStyles(
  columns: number,
  crossStart: number,
  crossEnd: number,
  columnWrapperStyle: StyleProp<ViewStyle>,
  horizontal: boolean
): ViewStyle[] | null {
  const wrapper = (StyleSheet.flatten(columnWrapperStyle) ?? {}) as ViewStyle;
  const count = Math.max(1, columns);
  const gap = count > 1 ? numberOr(wrapper.columnGap, wrapper.gap) : 0;
  const start =
    crossStart +
    (count > 1
      ? numberOr(
          wrapper.paddingLeft,
          wrapper.paddingStart,
          wrapper.paddingHorizontal,
          wrapper.padding
        )
      : 0);
  const end =
    crossEnd +
    (count > 1
      ? numberOr(
          wrapper.paddingRight,
          wrapper.paddingEnd,
          wrapper.paddingHorizontal,
          wrapper.padding
        )
      : 0);
  if (start === 0 && end === 0 && gap === 0) return null;
  const extra = start + end + gap * (count - 1);
  const styles: ViewStyle[] = [];
  for (let column = 0; column < count; column++) {
    const before = start + column * (gap - extra / count);
    const after = extra / count - before;
    styles.push(
      horizontal
        ? { paddingTop: before, paddingBottom: after }
        : { paddingLeft: before, paddingRight: after }
    );
  }
  return styles;
}
