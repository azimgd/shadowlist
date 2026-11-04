/*
 * What this package uses of react-native-tty, for tsc. The platform lives in the ream repo and is
 * bundled by the consumer, so nothing here can name its source: react-native-tty's own native.ts
 * and host.ts are what these lines repeat.
 */
declare module 'react-native-tty/native' {
  export interface KeyEvent {
    key: string;
  }
  export interface PointerEvent {
    kind: 'down' | 'move' | 'up';
    x: number;
    y: number;
  }
  export interface WheelEvent {
    x: number;
    y: number;
    deltaX: number;
    deltaY: number;
  }
  export interface NativeView {
    readonly tag: number;
    readonly props: Record<string, any>;
    readonly frame: { x: number; y: number; width: number; height: number };
    invalidate(): void;
    dispatchEvent(name: string, nativeEvent: unknown): void;
    focus(): void;
    blur(): void;
    isFocused(): boolean;
  }
  export interface ViewManager {
    create?(view: NativeView): void;
    update?(
      view: NativeView,
      props: Record<string, any>,
      previous: Record<string, any>
    ): void;
    paint(view: NativeView): void;
    command?(view: NativeView, command: string, args: unknown[]): void;
    key?(view: NativeView, event: KeyEvent): boolean;
    pointer?(view: NativeView, event: PointerEvent): boolean;
    wheel?(view: NativeView, event: WheelEvent): boolean;
    destroy?(view: NativeView): void;
  }
  export function registerViewManager(name: string, manager: ViewManager): void;
  export function registerNativeModule(name: string, module: object): void;
}

declare module 'react-native-tty/host' {
  export interface TestHost {
    settle(): Promise<void>;
    snapshot(): string;
    press(
      key: string,
      modifiers?: {
        shiftKey?: boolean;
        ctrlKey?: boolean;
        altKey?: boolean;
        metaKey?: boolean;
      }
    ): void;
    send(event: object): void;
    sink: {
      cell(x: number, y: number): { text: string; node: number };
    };
  }
  export function createTestHost(size?: {
    columns?: number;
    rows?: number;
  }): TestHost;
}
