/*
 * Reply links come from model output. By default we only open ones that go to a browser
 * or mail client. Phone, SMS or deep links could start an action the reader never asked for.
 * isSafeUrl in the core's utils/MarkdownDocument.hpp is the same rule, and vectors.test holds the two
 * equal.
 */
const SAFE_SCHEME = /^(https?|mailto):/i;

export const isSafeUrl = (url: string) => SAFE_SCHEME.test(url.trim());
