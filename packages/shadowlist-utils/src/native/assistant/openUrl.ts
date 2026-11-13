import { Linking } from 'react-native';
import { isSafeUrl } from './isSafeUrl';

/*
 * Opens a link that isSafeUrl lets through. Pass onOpenLink to allow more. If the OS can't open
 * the link, stay put.
 */
export const openUrl = (url: string) => {
  if (!isSafeUrl(url)) return;
  Linking.openURL(url.trim()).catch(() => {});
};
