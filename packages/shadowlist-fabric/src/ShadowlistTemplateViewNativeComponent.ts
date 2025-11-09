import { codegenNativeComponent, type ViewProps } from 'react-native';

interface NativeProps extends ViewProps {
  templateType: string;
}

export default codegenNativeComponent<NativeProps>('ShadowlistTemplateView');
