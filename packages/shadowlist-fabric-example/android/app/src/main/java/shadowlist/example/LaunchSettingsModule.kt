package shadowlist.example

import android.os.Bundle
import com.facebook.react.BaseReactPackage
import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.NativeModule
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.ReactMethod
import com.facebook.react.bridge.WritableMap
import com.facebook.react.module.model.ReactModuleInfo
import com.facebook.react.module.model.ReactModuleInfoProvider

/*
 * Launch settings for scripted runs, the Android side of the iOS launch arguments:
 * adb shell am start -n shadowlist.example/.MainActivity --es SLRoute Feed --es SLCount 1000
 * MainActivity copies the intent extras here, and JS reads them once at startup (launchSettings.ts).
 */
object LaunchSettings {
  @Volatile var values: Map<String, String> = emptyMap()

  fun capture(extras: Bundle?) {
    if (extras == null) return
    values = extras.keySet().filter { it.startsWith("SL") }.associateWith {
      @Suppress("DEPRECATION")
      extras.get(it)?.toString() ?: ""
    }
  }
}

class LaunchSettingsModule(context: ReactApplicationContext) : ReactContextBaseJavaModule(context) {
  override fun getName(): String = NAME

  @ReactMethod(isBlockingSynchronousMethod = true)
  fun getAll(): WritableMap {
    val map = Arguments.createMap()
    LaunchSettings.values.forEach { (key, value) -> map.putString(key, value) }
    return map
  }

  companion object {
    const val NAME = "SLLaunchSettings"
  }
}

class LaunchSettingsPackage : BaseReactPackage() {
  override fun getModule(name: String, reactContext: ReactApplicationContext): NativeModule? =
      if (name == LaunchSettingsModule.NAME) LaunchSettingsModule(reactContext) else null

  override fun getReactModuleInfoProvider(): ReactModuleInfoProvider = ReactModuleInfoProvider {
    mapOf(
        LaunchSettingsModule.NAME to
            ReactModuleInfo(
                LaunchSettingsModule.NAME,
                LaunchSettingsModule::class.java.name,
                false, // canOverrideExistingModule
                false, // needsEagerInit
                false, // isCxxModule
                false, // isTurboModule
            ))
  }
}
