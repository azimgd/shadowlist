import {
  codegenNativeComponent,
  type ViewProps,
  type CodegenTypes,
} from 'react-native';

interface NativeProps extends ViewProps {
  index: CodegenTypes.Int32;
  /*
   * The element's data key. Native resolves it to the current core element via
   * Container::findElementIndexByKey, so a mounted view always maps to the right row even
   * if its index has shifted (prepend/insert/reorder). `index` is kept as a debug/fallback.
   */
  elementKey?: string;
}

export default codegenNativeComponent<NativeProps>('ShadowListElementView');
