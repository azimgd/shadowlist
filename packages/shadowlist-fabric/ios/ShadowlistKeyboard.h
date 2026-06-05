// Generated TurboModule spec for this codegen project (codegenConfig name
// "ShadowlistViewSpec", type "all"). It declares the NativeShadowlistKeyboardSpec
// protocol and the NativeShadowlistKeyboardSpecBase class, which provides
// emitOnKeyboardMove: (via the codegen EventEmitter) - so we subclass the base AND
// conform to the protocol. This header is ObjC++ (the spec carries C++ types); import
// it only from .mm translation units.
#import <ShadowlistViewSpec/ShadowlistViewSpec.h>

/*
 * TurboModule that observes the system keyboard and streams its frame to JS as
 * onKeyboardMove events (see NativeShadowlistKeyboard.ts / useKeyboardAnimation).
 *
 * iOS notes: the public keyboard APIs expose the frame through notifications
 * (begin/end + duration + curve), not on every vsync. So animated open/close is
 * sampled by a CADisplayLink that interpolates over the system's reported duration,
 * which keeps it in step with the keyboard. True per-frame tracking of an interactive
 * (scroll-to-dismiss) drag is not delivered by notifications; that needs the
 * inputAccessoryView tracking technique and can be layered on later.
 */
@interface ShadowlistKeyboard : NativeShadowlistKeyboardSpecBase <NativeShadowlistKeyboardSpec>
@end
