import { NativeModules, Platform, Settings } from 'react-native';

/*
 * Launch settings for scripted runs: SLRoute Chat, SLLatency 0,0, SLSendFailureRate 0.5,
 * SLTheme light|dark, SLDebug 1 for debug buttons, captions and status lines, and SLCount N
 * for benchmark screens that open holding exactly N rows (see benchCount).
 * iOS reads launch arguments (-SLRoute Chat). Android reads intent extras
 * (adb shell am start -n shadowlist.example/.MainActivity --es SLRoute Chat), which
 * MainActivity hands over through the SLLaunchSettings native module.
 */
const androidSettings: Record<string, string> = (() => {
  if (Platform.OS !== 'android') return {};
  try {
    return NativeModules.SLLaunchSettings?.getAll?.() ?? {};
  } catch {
    return {};
  }
})();

export function launchSetting(key: string): string | undefined {
  if (Platform.OS === 'android') return androidSettings[key];
  if (Platform.OS !== 'ios') return undefined;
  const value: unknown = Settings.get(key);
  return value == null ? undefined : String(value);
}

export const DEBUG = launchSetting('SLDebug') === '1';

/*
 * SLCount: the Feed, FeedNative, Chat, ChatNative, SectionList and Masonry data sources
 * seed and serve their first page with exactly this many rows. Undefined keeps the defaults.
 */
export const benchCount: number | undefined = (() => {
  const count = Number(launchSetting('SLCount'));
  return Number.isInteger(count) && count > 0 ? count : undefined;
})();
