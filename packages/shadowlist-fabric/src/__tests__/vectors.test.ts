import { describe, it } from '@jest/globals';
import {
  expectEveryVectorFileReplayed,
  replayVectors,
} from '../__fixtures__/vectors';
import {
  grownMountedRange,
  initialMountedRange,
  shouldReseedFromOffsetIndex,
  unionRangeIndices,
  type MountedRange,
} from '../virtualizer/useMountedRange';

/*
 * The vector files of packages/shadowlist-core-tests/vectors, replayed through the functions
 * here. tests_host_vectors.cpp replays the same files through packages/shadowlist-core.
 * A change of behavior on either side fails its replay. After a change that is meant, on both
 * sides, UPDATE_VECTORS=1 make test writes the new outputs.
 */
describe('vectors', () => {
  it('mounted-range.json', () => {
    replayVectors('mounted-range.json', (vector) => {
      switch (vector.function) {
        case 'initialMountedRange': {
          const input = vector.input as {
            size: number;
            initialCount: number;
            inverted: boolean;
            target: number;
            overscanRows: number;
            viewPosition: number;
          };
          return initialMountedRange(
            input.size,
            input.initialCount,
            input.inverted,
            input.target,
            input.overscanRows,
            input.viewPosition
          );
        }
        case 'grownMountedRange': {
          const input = vector.input as {
            edges: MountedRange;
            lowAtStart: boolean;
            highAtEnd: boolean;
            size: number;
            overscanRowsLeading: number;
            followTail: boolean;
          };
          return grownMountedRange(
            input.edges.low,
            input.edges.high,
            input.lowAtStart,
            input.highAtEnd,
            input.size,
            input.overscanRowsLeading,
            input.followTail
          );
        }
        case 'unionRangeIndices': {
          const input = vector.input as {
            first: MountedRange;
            second: MountedRange;
          };
          return unionRangeIndices(input.first, input.second);
        }
        case 'shouldReseedFromIndex': {
          const input = vector.input as {
            previousIndex: number;
            nextIndex: number;
          };
          return shouldReseedFromOffsetIndex(
            input.previousIndex,
            input.nextIndex
          );
        }
        default:
          throw new Error(`${vector.name} names no function`);
      }
    });
  });

  it('every file in the folder is replayed', () => {
    expectEveryVectorFileReplayed();
  });
});
