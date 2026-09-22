import { Linking } from 'react-native';

/*
 * Reply links come from model output, so by default we only open ones that go to a browser
 * or mail client. Phone, SMS or deep links could start an action the reader never asked for.
 * Pass onOpenLink to allow more.
 */
const SAFE_SCHEME = /^(https?|mailto):/i;

const isSafeUrl = (url: string) => SAFE_SCHEME.test(url.trim());

/*
 * If the OS can't open the link, stay put.
 */
export const openUrl = (url: string) => {
  if (!isSafeUrl(url)) return;
  Linking.openURL(url.trim()).catch(() => {});
};
