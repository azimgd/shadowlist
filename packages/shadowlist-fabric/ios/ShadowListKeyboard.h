// ObjC++ spec header; import only from .mm translation units.
#import <ShadowListViewSpec/ShadowListViewSpec.h>

// Observes the system keyboard and streams its frame to JS as onKeyboardMove events.
@interface ShadowListKeyboard : NativeShadowListKeyboardSpecBase <NativeShadowListKeyboardSpec>
@end
