#import "ShadowListEngineFlags.h"

#include <react/featureflags/ReactNativeFeatureFlags.h>
#include <react/featureflags/ReactNativeFeatureFlagsOverridesOSSStable.h>

#include <memory>

using namespace facebook::react;

namespace {

/*
 * React Native's stable flags plus the ones below. Android only flags live in
 * MainApplication.kt.
 */
class ShadowListEngineFlags final : public ReactNativeFeatureFlagsOverridesOSSStable {
public:
  explicit ShadowListEngineFlags(bool commitBranching) : commitBranching_(commitBranching) {}

  // The mount diff looks children up in a hash map instead of a small linear map.
  bool useUnorderedMapInDifferentiator() override
  {
    return true;
  }

  /*
   * Experimental, only with -SLCommitBranching YES. Branching keeps JS commits from
   * starving behind native state commits, and a thread that keeps losing the commit race
   * takes a lock and tries again.
   */
  bool enableFabricCommitBranching() override
  {
    return commitBranching_;
  }

  bool preventShadowTreeCommitExhaustion() override
  {
    return commitBranching_;
  }

private:
  bool commitBranching_;
};

}

void ShadowListApplyEngineFlags(void)
{
  NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
  BOOL enabled = [defaults objectForKey:@"SLEngineFlags"] == nil || [defaults boolForKey:@"SLEngineFlags"];
  if (!enabled) {
    NSLog(@"[SL] engine flags off");
    return;
  }
  BOOL commitBranching = [defaults boolForKey:@"SLCommitBranching"];
  /*
   * The factory already set React Native's flags, so replace them. This is only safe
   * before React Native starts, since it swaps the flag store under any reader.
   */
  ReactNativeFeatureFlags::dangerouslyForceOverride(std::make_unique<ShadowListEngineFlags>(commitBranching));
  NSLog(@"[SL] engine flags on, commit branching %@", commitBranching ? @"on" : @"off");
}
