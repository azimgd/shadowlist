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
  elementStyle?: ViewStyle;
  initialElementsSize?: number;
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
  elementStyle,
  initialElementsSize = 56,
  testID,
  accessibilityLabel,
  accessibilityRole,
  listRef,
}: TemplateListProps<R>) => {
  const renderElement = useCallback(
    ({ element }: { element: R }) => {
      const render = (
        templates as unknown as Record<string, (row: R) => ReactElement | null>
      )[element.kind];
      return render?.(element) ?? <></>;
    },
    [templates]
  );
  return (
    <ShadowList
      ref={listRef}
      data={data}
      renderElement={renderElement}
      horizontal={horizontal}
      style={style}
      elementStyle={elementStyle}
      initialElementsSize={initialElementsSize}
      testID={testID}
      accessibilityLabel={accessibilityLabel}
      accessibilityRole={accessibilityRole}
    />
  );
};
