import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import codegenNativeCommands from 'react-native/Libraries/Utilities/codegenNativeCommands';
import type { ViewProps } from 'react-native';
import type {
  Int32,
  DirectEventHandler,
} from 'react-native/Libraries/Types/CodegenTypes';

export type ScrollToIndexOptions = {
  index: number;
  animated?: boolean;
};

export type ScrollToOffsetOptions = {
  offset: number;
  animated?: boolean;
};

export type OnVisibleChange = {
  visibleStartIndex: Int32;
  visibleEndIndex: Int32;
};

export type OnStartReached = {
  distanceFromStart: Int32;
};

export type OnEndReached = {
  distanceFromEnd: Int32;
};

export type OnScroll = {
  contentSize: {
    width: Int32;
    height: Int32;
  };
  contentOffset: {
    x: Int32;
    y: Int32;
  };
};

export type OnViewableItemsChanged = {
  viewableItems: {
    key: string;
    index: Int32;
    isViewable: boolean;
    origin: {
      x: Int32;
      y: Int32;
    };
    size: {
      width: Int32;
      height: Int32;
    };
  }[];
};

export interface SLContainerNativeProps extends ViewProps {
  data: string;
  inverted?: boolean;
  horizontal?: boolean;
  initialNumToRender?: Int32;
  windowSize?: Int32;
  initialScrollIndex?: Int32;
  numColumns?: Int32;
  onVisibleChange?: DirectEventHandler<Readonly<OnVisibleChange>>;
  onStartReached?: DirectEventHandler<Readonly<OnStartReached>>;
  onEndReached?: DirectEventHandler<Readonly<OnEndReached>>;
  onScroll?: DirectEventHandler<Readonly<OnScroll>>;
  onViewableItemsChanged?: DirectEventHandler<Readonly<OnViewableItemsChanged>>;
}

export interface SLContainerNativeCommands {
  scrollToIndex: (
    // @ts-ignore
    viewRef: React.ElementRef<React.ComponentType>,
    index: Int32,
    animated: boolean
  ) => void;
  scrollToOffset: (
    // @ts-ignore
    viewRef: React.ElementRef<React.ComponentType>,
    offset: Int32,
    animated: boolean
  ) => void;
}

export const Commands = codegenNativeCommands<SLContainerNativeCommands>({
  supportedCommands: ['scrollToIndex', 'scrollToOffset'],
});

export default codegenNativeComponent<SLContainerNativeProps>('SLContainer', {
  interfaceOnly: true,
});
