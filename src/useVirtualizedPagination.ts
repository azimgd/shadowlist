import React from 'react';
import { type SLContainerNativeProps } from './SLContainerNativeComponent';

const DISABLE_ON_START_REACHED = true;

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
  const PER_RENDER = 10;
  const inProgress = React.useRef(false);
  const [nextData, setNextData] = React.useState(data.slice(0, PER_RENDER));

  React.useLayoutEffect(() => {
    inProgress.current = false;
  }, [nextData]);

  const nextOnStartReached = React.useCallback<
    NonNullable<SLContainerNativeProps['onStartReached']>
  >(
    (args) => {
      if (DISABLE_ON_START_REACHED) return;

      args.persist();
      setNextData((prevData) => {
        if (inProgress.current) return prevData;

        const dataLengthDiff = data.length - prevData.length;
        if (dataLengthDiff > 0) {
          inProgress.current = true;
          return data.slice(0, prevData.length + PER_RENDER);
        }

        if (typeof onStartReached === 'function') {
          onStartReached(args);
        }

        return prevData;
      });
    },
    [onStartReached, data]
  );

  const nextOnEndReached = React.useCallback<
    NonNullable<SLContainerNativeProps['onEndReached']>
  >(
    (args) => {
      args.persist();
      setNextData((prevData) => {
        if (inProgress.current) return prevData;

        const dataLengthDiff = data.length - prevData.length;
        if (dataLengthDiff > 0) {
          inProgress.current = true;
          return data.slice(0, prevData.length + PER_RENDER);
        }

        if (typeof onEndReached === 'function') {
          onEndReached(args);
        }

        return prevData;
      });
    },
    [onEndReached, data]
  );

  const nextOnVisibleChange = React.useCallback<
    NonNullable<SLContainerNativeProps['onVisibleChange']>
  >((args) => {
    args.nativeEvent;
  }, []);

  return {
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
