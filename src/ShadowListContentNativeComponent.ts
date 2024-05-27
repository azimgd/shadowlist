import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import type { ViewProps } from 'react-native';

export interface NativeProps extends ViewProps {
  inverted?: boolean;
  horizontal?: boolean;
  hasListHeaderComponent?: boolean;
  hasListFooterComponent?: boolean;
}

export default codegenNativeComponent<NativeProps>('ShadowListContent', {
  interfaceOnly: true,
});
