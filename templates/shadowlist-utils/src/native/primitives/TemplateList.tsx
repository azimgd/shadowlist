import { useCallback, type ReactElement, type Ref } from 'react';
import type { AccessibilityRole, ViewStyle } from 'react-native';
import { ShadowList, type ShadowListCommands } from 'shadowlist';

export interface TemplateRow {
  id: string;
  kind: string;
}

export type TemplateMap<R extends TemplateRow> = {
  [K in R['kind']]: (row: Extract<R, { kind: K }>) => ReactElement | null;
};

export interface TemplateListProps<R extends TemplateRow> {
  data: ReadonlyArray<R>;
  templates: TemplateMap<R>;
  horizontal?: boolean;
  style?: ViewStyle;
  itemStyle?: ViewStyle;
  initialNumToRender?: number;
  testID?: string;
  accessibilityLabel?: string;
  accessibilityRole?: AccessibilityRole;
  listRef?: Ref<ShadowListCommands>;
}

/*
 * A ShadowList of mixed rows. Each row is a plain object with a `kind`, and `templates` maps
 * every kind to its renderer. A screen describes what it shows as data and defines each row
 * in one place.
 */
export const TemplateList = <R extends TemplateRow>({
  data,
  templates,
  horizontal = false,
  style,
  itemStyle,
  initialNumToRender = 56,
  testID,
  accessibilityLabel,
  accessibilityRole,
  listRef,
}: TemplateListProps<R>) => {
  const renderItem = useCallback(
    ({ item }: { item: R }) => {
      const render = (
        templates as unknown as Record<string, (row: R) => ReactElement | null>
      )[item.kind];
      return render?.(item) ?? <></>;
    },
    [templates]
  );
  return (
    <ShadowList
      ref={listRef}
      data={data}
      renderItem={renderItem}
      horizontal={horizontal}
      style={style}
      itemStyle={itemStyle}
      initialNumToRender={initialNumToRender}
      testID={testID}
      accessibilityLabel={accessibilityLabel}
      accessibilityRole={accessibilityRole}
    />
  );
};
