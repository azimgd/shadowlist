/*
 * What applyChanges found. Deleted and moved from are indices in the previous data, inserted,
 * moved to and reloaded indices in the new data.
 */
export class ShadowListKitListChanges {
  constructor(
    readonly deletedIndices: ReadonlyArray<number>,
    readonly insertedIndices: ReadonlyArray<number>,
    readonly movedFromIndices: ReadonlyArray<number>,
    readonly movedToIndices: ReadonlyArray<number>,
    readonly reloadedIndices: ReadonlyArray<number>
  ) {}

  get isEmpty(): boolean {
    return (
      this.deletedIndices.length === 0 &&
      this.insertedIndices.length === 0 &&
      this.movedFromIndices.length === 0 &&
      this.reloadedIndices.length === 0
    );
  }

  toString(): string {
    return (
      `ShadowListKitListChanges(deleted=${this.deletedIndices.length}, inserted=${this.insertedIndices.length}, ` +
      `moved=${this.movedFromIndices.length}, reloaded=${this.reloadedIndices.length})`
    );
  }
}
