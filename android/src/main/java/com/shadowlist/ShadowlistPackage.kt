package com.shadowlist

import com.facebook.react.BaseReactPackage
import com.facebook.react.bridge.NativeModule
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.uimanager.ViewManager
import com.facebook.react.module.model.ReactModuleInfo
import com.facebook.react.module.model.ReactModuleInfoProvider

class ShadowlistPackage : BaseReactPackage() {
  override fun createViewManagers(reactContext: ReactApplicationContext): List<ViewManager<*, *>> {
    val viewManagers = mutableListOf<ViewManager<*, *>>()
    viewManagers.add(SLContainerManager())
    viewManagers.add(SLContentManager())
    viewManagers.add(SLElementManager())
    return viewManagers
  }

  override fun getModule(name: String, reactContext: ReactApplicationContext): NativeModule? {
    return if (name == ShadowlistModule.NAME) {
      ShadowlistModule(reactContext)
    } else {
      null
    }
  }

  override fun getReactModuleInfoProvider(): ReactModuleInfoProvider {
    return ReactModuleInfoProvider {
      val moduleInfos = mutableMapOf<String, ReactModuleInfo>()
      moduleInfos[ShadowlistModule.NAME] = ReactModuleInfo(
        ShadowlistModule.NAME,
        ShadowlistModule.NAME,
        false,
        false,
        false,
        true
      )
      moduleInfos
    }
  }
}
