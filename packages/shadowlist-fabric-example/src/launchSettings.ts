import { Platform, Settings } from 'react-native';

/*
 * iOS launch arguments for scripted runs: -SLRoute Chat, -SLLatency 0,0, -SLSendFailureRate 0.5,
 * -SLTheme light|dark, and -SLDebug 1 for debug buttons, captions and status lines.
 */
export function launchSetting(key: string): string | undefined {
  if (Platform.OS !== 'ios') return undefined;
  const value: unknown = Settings.get(key);
  return value == null ? undefined : String(value);
}

export const DEBUG = launchSetting('SLDebug') === '1';
