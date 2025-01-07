import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import type { ViewProps } from 'react-native';

export interface SLElementNativeProps extends ViewProps {
  uniqueId: string;
}

export default codegenNativeComponent<SLElementNativeProps>('SLElement');
