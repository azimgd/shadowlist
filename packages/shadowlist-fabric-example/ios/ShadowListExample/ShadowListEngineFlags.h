#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * Turns on React Native engine flags that make list commits cheaper. Call it right after
 * creating RCTReactNativeFactory, before React Native starts.
 *
 * On by default. Launch with -SLEngineFlags NO to keep React Native's own flags, for an A/B
 * run. Launch with -SLCommitBranching YES to also try the commit branching flags, which are
 * experimental and stay off otherwise.
 */
FOUNDATION_EXPORT void ShadowListApplyEngineFlags(void);

NS_ASSUME_NONNULL_END
