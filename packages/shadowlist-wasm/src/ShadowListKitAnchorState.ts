/*
 * A scroll position that survives data changes: the key of the row at the viewport start and
 * how far the viewport start is past that row's leading edge, in CSS pixels.
 */
export class ShadowListKitAnchorState {
  constructor(
    readonly key: string,
    readonly offset: number
  ) {}

  equals(other: ShadowListKitAnchorState | null): boolean {
    return (
      other != null && other.key === this.key && other.offset === this.offset
    );
  }

  toString(): string {
    return `ShadowListKitAnchorState(key=${this.key}, offset=${this.offset})`;
  }
}
