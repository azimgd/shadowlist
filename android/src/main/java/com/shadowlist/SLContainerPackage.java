package com.shadowlist;

import com.facebook.react.BaseReactPackage;
import com.facebook.react.bridge.NativeModule;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.uimanager.ViewManager;
import com.facebook.react.module.model.ReactModuleInfo;
import com.facebook.react.module.model.ReactModuleInfoProvider;


import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.HashMap;
import java.util.Map;

public class SLContainerPackage extends BaseReactPackage {
  @Override
  public List<ViewManager> createViewManagers(ReactApplicationContext reactContext) {
    List<ViewManager> viewManagers = new ArrayList<>();
    viewManagers.add(new SLContainerManager());
    viewManagers.add(new SLElementManager());
    return viewManagers;
  }

  @Override
  public List<NativeModule> createNativeModules(ReactApplicationContext reactContext) {
    return Collections.emptyList();
  }

  @Override
  public NativeModule getModule(String name, ReactApplicationContext reactContext) {
    if (name.equals(SLModule.NAME)) {
      return new SLModule(reactContext);
    } else {
      return null;
    }
  }

  @Override
  public ReactModuleInfoProvider getReactModuleInfoProvider() {
    return () -> {
      final Map<String, ReactModuleInfo> moduleInfos = new HashMap<>();
      moduleInfos.put(
        SLModule.NAME,
        new ReactModuleInfo(
          SLModule.NAME,
          SLModule.NAME,
          false,  // canOverrideExistingModule
          false,  // needsEagerInit
          true,   // hasConstants
          false,  // isCxxModule
          true    // isTurboModule
        ));
      return moduleInfos;
    };
  }
}
