import { codegenNativeComponent, type ViewProps } from 'react-native';

export interface NativeProps extends ViewProps {
  templateType: string;
}

export default codegenNativeComponent<NativeProps>('ShadowListTemplateView');
