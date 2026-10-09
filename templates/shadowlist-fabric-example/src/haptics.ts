import { trigger } from 'react-native-haptic-feedback';

const OPTIONS = {
  enableVibrateFallback: false,
  ignoreAndroidSystemSettings: false,
};

export const haptics = {
  send: () => trigger('impactLight', OPTIONS),
  remove: () => trigger('notificationWarning', OPTIONS),
  drop: () => trigger('impactMedium', OPTIONS),
  selection: () => trigger('selection', OPTIONS),
};
