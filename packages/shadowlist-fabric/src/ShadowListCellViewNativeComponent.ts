import {
  codegenNativeComponent,
  codegenNativeCommands,
  type ViewProps,
  type CodegenTypes,
  type ColorValue,
} from 'react-native';

/*
 * A swipe action button. backgroundColor is a processed color, see processColor, which keeps
 * PlatformColor and DynamicColorIOS. style is 'normal' or 'destructive'.
 */
export type NativeSwipeAction = Readonly<{
  title: string;
  backgroundColor: ColorValue;
  style: string;
}>;

export type NativeMenuAction = Readonly<{
  title: string;
  style: string;
  disabled: boolean;
  systemImage: string;
}>;

export type OnSwipeAction = {
  leading: boolean;
  actionIndex: CodegenTypes.Int32;
  fullSwipe: boolean;
};

export type OnContextMenuAction = {
  actionIndex: CodegenTypes.Int32;
};

interface NativeProps extends ViewProps {
  rowKey?: string;
  leadingSwipeActions?: ReadonlyArray<NativeSwipeAction>;
  trailingSwipeActions?: ReadonlyArray<NativeSwipeAction>;
  leadingFullSwipe?: CodegenTypes.WithDefault<boolean, true>;
  trailingFullSwipe?: CodegenTypes.WithDefault<boolean, true>;
  contextMenuTitle?: string;
  contextMenuActions?: ReadonlyArray<NativeMenuAction>;
  readonly onSwipeAction?: CodegenTypes.DirectEventHandler<OnSwipeAction>;
  readonly onContextMenuAction?: CodegenTypes.DirectEventHandler<OnContextMenuAction>;
}

type ShadowListCellViewComponentType = ReturnType<
  typeof codegenNativeComponent<NativeProps>
>;

interface NativeCommands {
  closeFullSwipe: (
    viewRef: React.ElementRef<ShadowListCellViewComponentType>
  ) => void;
}

export const Commands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['closeFullSwipe'],
});

export default codegenNativeComponent<NativeProps>('ShadowListCellView');
