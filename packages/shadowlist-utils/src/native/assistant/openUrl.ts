import { Linking } from 'react-native';

/*
 * Links in a reply come from model output, so the default handler only opens schemes that
 * leave the app for a browser or mail client. Anything else (tel:, sms:, an app's own deep
 * links) could trigger an action the reader never asked for; pass `onOpenLink` to allow more.
 */
const SAFE_SCHEME = /^(https?|mailto):/i;

const isSafeUrl = (url: string) => SAFE_SCHEME.test(url.trim());

// A link the OS can't open has no better fallback than staying put.
export const openUrl = (url: string) => {
  if (!isSafeUrl(url)) return;
  Linking.openURL(url.trim()).catch(() => {});
};
