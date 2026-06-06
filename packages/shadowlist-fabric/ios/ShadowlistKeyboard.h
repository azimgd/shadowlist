// ObjC++ spec header; import only from .mm translation units.
#import <ShadowlistViewSpec/ShadowlistViewSpec.h>

// Observes the system keyboard and streams its frame to JS as onKeyboardMove events.
@interface ShadowlistKeyboard : NativeShadowlistKeyboardSpecBase <NativeShadowlistKeyboardSpec>
@end
