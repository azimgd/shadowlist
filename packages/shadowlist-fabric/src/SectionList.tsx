import type { Ref, ReactElement } from 'react';
import { useMemo, useCallback, useRef, forwardRef } from 'react';
import ShadowList from './ShadowList';
import { slTrace, slTraceEnabled, useStableElement } from './virtualizer';
import {
  flattenSections,
  type FlatRow,
  type SectionRows,
} from './virtualizer/sectionRows';
import type { ShadowListCommands, SectionListProps } from './types';

/*
 * ShadowList renders one flat list, so each section becomes a header row, its elements,
 * then a footer row. The header positions go into stickyHeaderIndices so native can pin them.
 */

function renderComponent(
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
}

function sameIndices(
  previous: number[] | undefined,
  next: number[]
): previous is number[] {
  return (
    previous !== undefined &&
    previous.length === next.length &&
    previous.every((value, index) => value === next[index])
  );
}

function toRowIds<ElementT, SectionT>(
  rows: ReadonlyArray<FlatRow<ElementT, SectionT>>,
  elementKeys: ReadonlyArray<string> | undefined,
  previous: string[] | undefined
): string[] | undefined {
  if (!elementKeys || elementKeys.length === 0) return undefined;
  const keys = new Set(elementKeys);
  const ids: string[] = [];
  for (const row of rows) {
    if (row.elementKey !== undefined && keys.has(row.elementKey)) {
      ids.push(row.id);
    }
  }
  // The same ids keep the old array, so native gets no new prop.
  if (
    previous !== undefined &&
    previous.length === ids.length &&
    previous.every((id, index) => id === ids[index])
  ) {
    return previous;
  }
  return ids;
}

function SectionListInner<ElementT, SectionT = object>(
  {
    sections,
    renderElement,
    renderSectionHeader,
    renderSectionFooter,
    keyExtractor,
    stickySectionHeadersEnabled = true,
    ItemSeparatorComponent,
    SectionSeparatorComponent,
    getElementSizeSpec,
    nonAnchorKeys,
    persistentKeys,
    ...rest
  }: SectionListProps<ElementT, SectionT>,
  ref: Ref<ShadowListCommands>
) {
  // The current sections, read by the renderers below so element rows don't carry the section.
  const sectionsRef = useRef(sections);
  sectionsRef.current = sections;

  /*
   * Rows from the last flatten, per section key, so an unchanged row keeps its old object.
   * The list mounts rows by identity, so without this any change to sections re-renders
   * every row. An unchanged section is reused whole.
   */
  const previousSectionsRef = useRef<
    Map<string, SectionRows<ElementT, SectionT>>
  >(new Map());
  const previousIndicesRef = useRef<number[] | undefined>(undefined);

  const { data, stickyHeaderIndices } = useMemo(() => {
    const { rows, stickyIndices, nextSections } = flattenSections(
      sections,
      keyExtractor,
      !!renderSectionHeader,
      !!renderSectionFooter,
      stickySectionHeadersEnabled,
      previousSectionsRef.current
    );
    previousSectionsRef.current = nextSections;

    /*
     * Header positions rarely move. A new array would force native to copy all props,
     * row keys included.
     */
    const indices = sameIndices(previousIndicesRef.current, stickyIndices)
      ? previousIndicesRef.current
      : stickyIndices;
    previousIndicesRef.current = indices;

    return { data: rows, stickyHeaderIndices: indices };
  }, [
    sections,
    keyExtractor,
    renderSectionHeader,
    renderSectionFooter,
    stickySectionHeadersEnabled,
  ]);

  const renderStickyHeaderOverlay = useCallback(
    (activeIndex: number) => {
      const row = data[activeIndex];
      if (!row || !renderSectionHeader) return null;
      const section = row.section ?? sectionsRef.current[row.sectionIndex];
      if (!section) return null;
      return renderSectionHeader({ section });
    },
    [data, renderSectionHeader]
  );

  const getRowSizeSpec = useMemo(
    () =>
      getElementSizeSpec
        ? (row: FlatRow<ElementT, SectionT>) => {
            const section = sectionsRef.current[row.sectionIndex];
            return row.type === 'element' && section
              ? getElementSizeSpec(
                  row.element as ElementT,
                  row.elementIndex as number,
                  section
                )
              : null;
          }
        : undefined,
    [getElementSizeSpec]
  );

  const previousNonAnchorRef = useRef<string[] | undefined>(undefined);
  const previousPersistentRef = useRef<string[] | undefined>(undefined);
  const rowNonAnchorKeys = useMemo(() => {
    const ids = toRowIds(data, nonAnchorKeys, previousNonAnchorRef.current);
    previousNonAnchorRef.current = ids;
    return ids;
  }, [data, nonAnchorKeys]);
  const rowPersistentKeys = useMemo(() => {
    const ids = toRowIds(data, persistentKeys, previousPersistentRef.current);
    previousPersistentRef.current = ids;
    return ids;
  }, [data, persistentKeys]);

  /*
   * Both separators go inside every row, so an inline element would rebuild every mounted
   * row on each caller render.
   */
  const elementSeparator = useStableElement(
    useMemo(
      () => renderComponent(ItemSeparatorComponent),
      [ItemSeparatorComponent]
    )
  );
  const sectionSeparator = useStableElement(
    useMemo(
      () => renderComponent(SectionSeparatorComponent),
      [SectionSeparatorComponent]
    )
  );

  const renderRow = useCallback(
    ({
      element: row,
    }: {
      element: FlatRow<ElementT, SectionT>;
      index: number;
    }) => {
      const section = row.section ?? sectionsRef.current[row.sectionIndex];

      if (row.type === 'sectionHeader') {
        return (section && renderSectionHeader?.({ section })) ?? <></>;
      }

      if (row.type === 'sectionFooter') {
        return (
          <>
            {section ? renderSectionFooter?.({ section }) : null}
            {row.isSectionBoundary ? sectionSeparator : null}
          </>
        );
      }

      const sectionRenderElement = section?.renderElement ?? renderElement;
      const content =
        section && sectionRenderElement
          ? (sectionRenderElement({
              element: row.element as ElementT,
              index: row.elementIndex as number,
              section,
            }) ?? null)
          : null;

      let separator: ReactElement | null = null;
      if (row.isSectionBoundary) {
        separator = sectionSeparator;
      } else if (!row.isLastInSection) {
        separator = elementSeparator;
      }

      return (
        <>
          {content}
          {separator}
        </>
      );
    },
    [
      renderElement,
      renderSectionHeader,
      renderSectionFooter,
      elementSeparator,
      sectionSeparator,
    ]
  );

  /*
   * Device trace only. Logs which input gave the row renderer a new identity. A renderer that
   * changes every commit rebuilds every mounted row, so check this first when a list
   * re-renders more rows than changed.
   */
  const traceDepsRef = useRef<ReadonlyArray<unknown>>([]);
  if (slTraceEnabled()) {
    const deps = [
      renderElement,
      renderSectionHeader,
      renderSectionFooter,
      elementSeparator,
      sectionSeparator,
      data,
    ];
    const names = [
      'renderElement',
      'renderSectionHeader',
      'renderSectionFooter',
      'itemSeparator',
      'sectionSeparator',
      'data',
    ];
    const changed = names.filter(
      (_name, index) => traceDepsRef.current[index] !== deps[index]
    );
    traceDepsRef.current = deps;
    if (changed.length > 0) {
      slTrace(`section-deps changed=${changed.join(',')}`);
    }
  }

  return (
    <ShadowList
      {...rest}
      ref={ref}
      data={data}
      renderElement={renderRow}
      stickyHeaderIndices={stickyHeaderIndices}
      renderStickyHeaderOverlay={renderStickyHeaderOverlay}
      getElementSizeSpec={getRowSizeSpec}
      nonAnchorKeys={rowNonAnchorKeys}
      persistentKeys={rowPersistentKeys}
    />
  );
}

const SectionList = forwardRef(SectionListInner) as <
  ElementT,
  SectionT = object,
>(
  props: SectionListProps<ElementT, SectionT> & {
    ref?: Ref<ShadowListCommands>;
  }
) => ReactElement;

export default SectionList;
