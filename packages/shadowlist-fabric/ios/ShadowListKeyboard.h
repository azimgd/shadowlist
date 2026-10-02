// This pulls in an ObjC++ spec header. Import it only from .mm files.
#import <React/RCTInvalidating.h>
#import <ShadowListViewSpec/ShadowListViewSpec.h>

/*
 * Watch the system keyboard and send its frame to JS as onKeyboardMove events.
 */
@interface ShadowListKeyboard : NativeShadowListKeyboardSpecBase <NativeShadowListKeyboardSpec, RCTInvalidating>
@end
