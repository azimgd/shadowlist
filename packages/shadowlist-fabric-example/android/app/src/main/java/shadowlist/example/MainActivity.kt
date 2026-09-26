package shadowlist.example

import android.os.Bundle
import com.facebook.react.ReactActivity
import com.facebook.react.ReactActivityDelegate
import com.facebook.react.defaults.DefaultNewArchitectureEntryPoint.fabricEnabled
import com.facebook.react.defaults.DefaultReactActivityDelegate

class MainActivity : ReactActivity() {

  /** The name of the root component registered from JavaScript. */
  override fun getMainComponentName(): String = "ShadowListExample"

  /** Keeps the SL* intent extras for JS, see [LaunchSettings]. */
  override fun onCreate(savedInstanceState: Bundle?) {
    LaunchSettings.capture(intent?.extras)
    super.onCreate(savedInstanceState)
  }

  /** Turns on the New Architecture through the [fabricEnabled] flag. */
  override fun createReactActivityDelegate(): ReactActivityDelegate =
      DefaultReactActivityDelegate(this, mainComponentName, fabricEnabled)
}
