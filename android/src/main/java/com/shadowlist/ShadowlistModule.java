package com.shadowlist;

import com.facebook.jni.HybridData;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.UIManager;
import com.facebook.react.fabric.FabricUIManager;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.common.UIManagerType;
import com.facebook.react.turbomodule.core.interfaces.BindingsInstallerHolder;
import com.facebook.react.turbomodule.core.interfaces.TurboModuleWithJSIBindings;
import java.util.Objects;

@ReactModule(name = ShadowlistModule.NAME)
public class ShadowlistModule extends NativeShadowlistSpec implements TurboModuleWithJSIBindings {

  public static final String NAME = "Shadowlist";

  public ShadowlistModule(ReactApplicationContext reactContext) {
    super(reactContext);
  }

  @Override
  public String getName() {
    return NAME;
  }

  native HybridData initHybrid();
  private native void createCommitHook(FabricUIManager fabricUIManager);
  private native BindingsInstallerHolder injectJSIBindings();

  @DoNotStrip
  @SuppressWarnings("unused")
  private HybridData mHybridData = initHybrid();

  @Override
  public void setup() {
    UIManager uiManager = UIManagerHelper.getUIManager(
      getReactApplicationContext(),
      UIManagerType.FABRIC
    );

    FabricUIManager fabricUIManager = (FabricUIManager)uiManager;
    createCommitHook(fabricUIManager);
  }

  @Override
  public BindingsInstallerHolder getBindingsInstaller() {
    return injectJSIBindings();
  }
}
