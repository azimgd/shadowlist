import React from 'react';
import { type SLContainerNativeProps } from './SLContainerNativeComponent';

type UseVirtualizedPaginationProps = {
  data: any[];
  onStartReached: SLContainerNativeProps['onStartReached'];
  onEndReached: SLContainerNativeProps['onEndReached'];
  onVisibleChange: SLContainerNativeProps['onVisibleChange'];
  virtualizedPaginationEnabled: boolean;
};

const useVirtualizedPagination = ({
  data,
  onStartReached,
  onEndReached,
  onVisibleChange,
  virtualizedPaginationEnabled,
}: UseVirtualizedPaginationProps) => {
  const inProgress = React.useRef(false);
  const [nextData, setNextData] = React.useState(data);
  const [visibleIndices, setVisibleIndices] = React.useState({
    visibleStartIndex: 0,
    visibleEndIndex: 10,
  });

  React.useLayoutEffect(() => {
    inProgress.current = false;
  }, [nextData]);

  const nextOnStartReached = React.useCallback<
    NonNullable<SLContainerNativeProps['onStartReached']>
  >(
    (args) => {
      args.persist();
      setNextData((prevData) => {
        if (typeof onStartReached === 'function') {
          onStartReached(args);
        }

        return prevData;
      });
    },
    [onStartReached]
  );

  const nextOnEndReached = React.useCallback<
    NonNullable<SLContainerNativeProps['onEndReached']>
  >(
    (args) => {
      args.persist();
      setNextData((prevData) => {
        if (typeof onEndReached === 'function') {
          onEndReached(args);
        }

        return prevData;
      });
    },
    [onEndReached]
  );

  const nextOnVisibleChange = React.useCallback<
    NonNullable<SLContainerNativeProps['onVisibleChange']>
  >(
    (args) => {
      args.persist();
      if (typeof onVisibleChange === 'function') {
        onVisibleChange(args);
      }

      setVisibleIndices({
        visibleStartIndex: args.nativeEvent.visibleStartIndex,
        visibleEndIndex: args.nativeEvent.visibleEndIndex,
      });
    },
    [onVisibleChange]
  );

  return {
    visibleIndices,
    nextData: virtualizedPaginationEnabled ? nextData : data,
    nextOnStartReached: virtualizedPaginationEnabled
      ? nextOnStartReached
      : onStartReached,
    nextOnEndReached: virtualizedPaginationEnabled
      ? nextOnEndReached
      : onEndReached,
    nextOnVisibleChange: virtualizedPaginationEnabled
      ? nextOnVisibleChange
      : onVisibleChange,
  };
};

export default useVirtualizedPagination;
