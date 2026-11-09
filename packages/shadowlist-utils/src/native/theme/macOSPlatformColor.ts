import * as RN from 'react-native';
import type { ColorValue } from 'react-native';

/*
 * `PlatformColor` is public on every React Native platform. `DynamicColorMacOS` and
 * `ColorWithSystemEffectMacOS` are exported by the react-native-macos index but not by the
 * iOS renderer. Naming them in an import would not typecheck against the iOS types this
 * repo builds with. Reach through the namespace instead and declare the shapes here: the
 * call sites stay plain, and an iOS or Android bundle gets undefined rather than a missing
 * export.
 */
type DynamicColorMacOSTuple = {
  light: ColorValue;
  dark: ColorValue;
  highContrastLight?: ColorValue;
  highContrastDark?: ColorValue;
};

type SystemEffectMacOS = 'disabled' | 'pressed' | 'deepPressed' | 'rollover';

const macOSColorMacros = RN as unknown as {
  DynamicColorMacOS?: (tuple: DynamicColorMacOSTuple) => ColorValue;
  ColorWithSystemEffectMacOS?: (
    color: ColorValue,
    effect: SystemEffectMacOS
  ) => ColorValue;
};

export type { DynamicColorMacOSTuple, SystemEffectMacOS };

/*
 * PlatformColor takes AppKit semantic color names. AppKit resolves one per view at draw
 * time. A control written with these colors follows Dark Mode, an increase-contrast
 * setting, and an inactive window without a re-render.
 *
 * The name list is a fallback chain rather than a blend: AppKit takes the first name it
 * recognizes. Name a color added in a newer system first and an older one after it, because
 * an unresolved name logs a redbox in Debug and paints transparent in Release.
 */
export const semantic = (...names: string[]): ColorValue =>
  RN.PlatformColor(...names);

/*
 * Picks one of four authored colors per appearance. Use it where AppKit has no semantic
 * color for the role; the high-contrast slots let a hand-mixed token react to increase
 * contrast the way an AppKit color does.
 */
export const dynamic = (tuple: DynamicColorMacOSTuple): ColorValue =>
  macOSColorMacros.DynamicColorMacOS?.(tuple) ?? tuple.light;

/*
 * AppKit derives a control's hover, pressed and disabled tint from its base color. The
 * effect rides on the resolved color and applies to a semantic base too.
 */
export const withEffect = (
  color: ColorValue,
  effect: SystemEffectMacOS
): ColorValue =>
  macOSColorMacros.ColorWithSystemEffectMacOS?.(color, effect) ?? color;
