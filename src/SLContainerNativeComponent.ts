import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import codegenNativeCommands from 'react-native/Libraries/Utilities/codegenNativeCommands';
import type { ViewProps } from 'react-native';
import type {
  Int32,
  Float,
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
  visibleStartOffset: Float;
  visibleEndOffset: Float;
};

export type OnStartReached = {
  distanceFromStart: Int32;
};

export type OnEndReached = {
  distanceFromEnd: Int32;
};

export interface SLContainerNativeProps extends ViewProps {
  inverted?: boolean;
  horizontal?: boolean;
  initialNumToRender?: Int32;
  initialScrollIndex?: Int32;
  onVisibleChange?: DirectEventHandler<Readonly<OnVisibleChange>>;
  onStartReached?: DirectEventHandler<Readonly<OnStartReached>>;
  onEndReached?: DirectEventHandler<Readonly<OnEndReached>>;
}

export interface SLContainerNativeCommands {
  scrollToIndex: (
    viewRef: React.ElementRef<React.ComponentType>,
    index: Int32,
    animated: boolean
  ) => void;
  scrollToOffset: (
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
