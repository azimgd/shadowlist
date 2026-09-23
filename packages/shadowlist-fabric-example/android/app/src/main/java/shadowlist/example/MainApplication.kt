package shadowlist.example

import android.app.Application
import android.util.Log
import com.facebook.react.PackageList
import com.facebook.react.ReactApplication
import com.facebook.react.ReactHost
import com.facebook.react.ReactNativeApplicationEntryPoint.loadReactNative
import com.facebook.react.defaults.DefaultReactHost.getDefaultReactHost
import com.facebook.react.internal.featureflags.ReactNativeFeatureFlags
import com.facebook.react.internal.featureflags.ReactNativeNewArchitectureFeatureFlagsDefaults

class MainApplication : Application(), ReactApplication {

  override val reactHost: ReactHost by lazy {
    getDefaultReactHost(
      context = applicationContext,
      packageList =
        PackageList(this).packages.apply {
          // Add packages here that autolinking can't find.
          add(LaunchSettingsPackage())
        },
    )
  }

  override fun onCreate() {
    super.onCreate()
    loadReactNative(this)
    applyEngineFlags()
  }

  /*
   * Turn on React Native engine flags that make list commits cheaper. loadReactNative already
   * set React Native's own flags, so this replaces them, which is only safe before React Native
   * starts. BuildConfig.SHADOWLIST_ENGINE_FLAGS turns it off for an A/B run, see build.gradle.
   */
  private fun applyEngineFlags() {
    if (!BuildConfig.SHADOWLIST_ENGINE_FLAGS) {
      Log.i("SL", "[SL] engine flags off")
      return
    }
    ReactNativeFeatureFlags.dangerouslyForceOverride(ShadowListEngineFlags(BuildConfig.SHADOWLIST_COMMIT_BRANCHING))
    Log.i("SL", "[SL] engine flags on, commit branching ${BuildConfig.SHADOWLIST_COMMIT_BRANCHING}")
  }
}

/*
 * React Native's stable Android flags plus the ones below. They match the stable release
 * level, which is what loadReactNative picks by default.
 */
private class ShadowListEngineFlags(private val commitBranching: Boolean) :
  ReactNativeNewArchitectureFeatureFlagsDefaults() {

  // Same as the stable release level.
  override fun useFabricInterop(): Boolean = true

  // The mount diff looks children up in a hash map instead of a small linear map.
  override fun useUnorderedMapInDifferentiator(): Boolean = true

  // Send only the props that changed since the last mounted node, merged per update.
  override fun enablePropsUpdateReconciliationAndroid(): Boolean = true

  override fun enableExclusivePropsUpdateAndroid(): Boolean = true

  override fun enableAccumulatedUpdatesInRawPropsAndroid(): Boolean = true

  // Lets view managers that opt in, like the list rows, reuse views.
  override fun enableViewRecycling(): Boolean = true

  /*
   * Experimental, only with SHADOWLIST_COMMIT_BRANCHING. Branching keeps JS commits from
   * starving behind native state commits, and a thread that keeps losing the commit race
   * takes a lock and tries again.
   */
  override fun enableFabricCommitBranching(): Boolean = commitBranching

  override fun preventShadowTreeCommitExhaustion(): Boolean = commitBranching
}
