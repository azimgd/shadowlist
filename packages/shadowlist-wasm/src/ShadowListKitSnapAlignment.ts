/*
 * Which edge of an item a snapped scroll position lines up with the viewport. The order
 * matches the core's SnapAlignment.
 */
export const ShadowListKitSnapAlignment = {
  START: 'start',
  CENTER: 'center',
  END: 'end',
} as const;

export type ShadowListKitSnapAlignment =
  (typeof ShadowListKitSnapAlignment)[keyof typeof ShadowListKitSnapAlignment];

const ORDINALS: Record<ShadowListKitSnapAlignment, number> = {
  start: 0,
  center: 1,
  end: 2,
};

/*
 * The core's SnapAlignment value. Anything else lines up the start.
 */
export function snapAlignmentOrdinal(
  alignment: ShadowListKitSnapAlignment
): number {
  return ORDINALS[alignment] ?? 0;
}
