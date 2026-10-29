import { forwardRef, type ComponentProps, type ComponentRef } from 'react';
import ShadowListTemplateViewNativeComponent from './ShadowListTemplateViewNativeComponent';

/*
 * The template types native places. Any other value mounts a view native never places.
 */
const TEMPLATE_TYPES = new Set(['header', 'footer', 'empty', 'sectionHeader']);

/*
 * The native template view for apps that build a list from the native components, which
 * warns in __DEV__ about a templateType native does not know.
 */
const ShadowListTemplateView = forwardRef<
  ComponentRef<typeof ShadowListTemplateViewNativeComponent>,
  ComponentProps<typeof ShadowListTemplateViewNativeComponent>
>(function CheckedTemplateView(props, ref) {
  if (__DEV__ && !TEMPLATE_TYPES.has(props.templateType)) {
    console.warn(
      `ShadowListTemplateView: unknown templateType '${props.templateType}'. Use header, ` +
        'footer, empty or sectionHeader.'
    );
  }
  return <ShadowListTemplateViewNativeComponent {...props} ref={ref} />;
});

export default ShadowListTemplateView;
