import type { Ref, ReactElement } from 'react';
import { useMemo, useCallback, useRef, forwardRef } from 'react';
import ShadowList from './ShadowList';
import { slTrace, slTraceEnabled, useStableElement } from './virtualizer';
import type {
  ShadowListCommands,
  SectionListProps,
  SectionListData,
} from './types';

/*
 * ShadowList renders one flat list, so each section becomes a header row, its elements,
 * then a footer row. The header positions go into stickyHeaderIndices so native can pin them.
 */

type FlatRowType = 'sectionHeader' | 'element' | 'sectionFooter';

interface FlatRow<ElementT, SectionT> {
  id: string;
  type: FlatRowType;
  sectionIndex: number;
  section?: SectionListData<ElementT, SectionT>;
  element?: ElementT;
  elementIndex?: number;
  elementKey?: string;
  isLastInSection?: boolean;
  isSectionBoundary?: boolean;
}

function renderComponent(
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
}

/*
 * Whether the row built for this position matches the mounted one. Compare everything the
 * row renders from, or a reused row would show old content.
 */
function sameRow<ElementT, SectionT>(
  previous: FlatRow<ElementT, SectionT> | undefined,
  next: FlatRow<ElementT, SectionT>
): previous is FlatRow<ElementT, SectionT> {
  return (
    previous !== undefined &&
    previous.type === next.type &&
    previous.sectionIndex === next.sectionIndex &&
    previous.section === next.section &&
    previous.element === next.element &&
    previous.elementIndex === next.elementIndex &&
    previous.elementKey === next.elementKey &&
    previous.isLastInSection === next.isLastInSection &&
    previous.isSectionBoundary === next.isSectionBoundary
  );
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
  elementKeys: ReadonlyArray<string> | undefined
): string[] | undefined {
  if (!elementKeys || elementKeys.length === 0) return undefined;
  const keys = new Set(elementKeys);
  return rows
    .filter((row) => row.elementKey !== undefined && keys.has(row.elementKey))
    .map((row) => row.id);
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
   * Rows from the last flatten, by id, so an unchanged row keeps its old object. The list
   * mounts rows by identity, so without this any change to sections re-renders every row.
   */
  const previousRowsRef = useRef<Map<string, FlatRow<ElementT, SectionT>>>(
    new Map()
  );
  const previousIndicesRef = useRef<number[] | undefined>(undefined);

  const { data, stickyHeaderIndices } = useMemo(() => {
    const rows: FlatRow<ElementT, SectionT>[] = [];
    const stickyIndices: number[] = [];
    const previousRows = previousRowsRef.current;
    const nextRows = new Map<string, FlatRow<ElementT, SectionT>>();

    const push = (row: FlatRow<ElementT, SectionT>) => {
      const previousRow = previousRows.get(row.id);
      const finalRow = sameRow(previousRow, row) ? previousRow : row;
      nextRows.set(finalRow.id, finalRow);
      rows.push(finalRow);
    };

    sections.forEach((section, sectionIndex) => {
      const sectionKey = section.key ?? `section-${sectionIndex}`;
      const sectionKeyExtractor = section.keyExtractor ?? keyExtractor;

      if (renderSectionHeader) {
        if (stickySectionHeadersEnabled) {
          stickyIndices.push(rows.length);
        }
        push({
          id: `sh:${sectionKey}`,
          type: 'sectionHeader',
          section,
          sectionIndex,
        });
      }

      const lastElementIndex = section.data.length - 1;
      section.data.forEach((element, elementIndex) => {
        const elementKey = sectionKeyExtractor
          ? sectionKeyExtractor(element, elementIndex)
          : ((element as { id?: string })?.id ?? `${elementIndex}`);
        const isLastInSection = elementIndex === lastElementIndex;
        push({
          id: `si:${sectionKey}:${elementKey}`,
          type: 'element',
          sectionIndex,
          element,
          elementIndex,
          elementKey,
          isLastInSection,
          isSectionBoundary:
            isLastInSection &&
            !renderSectionFooter &&
            sectionIndex < sections.length - 1,
        });
      });

      if (renderSectionFooter) {
        push({
          id: `sf:${sectionKey}`,
          type: 'sectionFooter',
          section,
          sectionIndex,
          isSectionBoundary: sectionIndex < sections.length - 1,
        });
      }
    });

    previousRowsRef.current = nextRows;

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

  const rowNonAnchorKeys = useMemo(
    () => toRowIds(data, nonAnchorKeys),
    [data, nonAnchorKeys]
  );
  const rowPersistentKeys = useMemo(
    () => toRowIds(data, persistentKeys),
    [data, persistentKeys]
  );

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
