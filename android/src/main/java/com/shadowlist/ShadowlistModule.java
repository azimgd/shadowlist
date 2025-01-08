package com.shadowlist;

import com.facebook.jni.HybridData;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.UIManager;
import com.facebook.react.fabric.FabricUIManager;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.common.UIManagerType;
import java.util.Objects;

@ReactModule(name = ShadowlistModule.NAME)
public class ShadowlistModule extends NativeShadowlistSpec {

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
  private native void injectJSIBindings(long jsiRuntime);

  @DoNotStrip
  @SuppressWarnings("unused")
  private HybridData mHybridData;

  @Override
  public void setup() {
    UIManager uiManager = UIManagerHelper.getUIManager(
      getReactApplicationContext(),
      UIManagerType.FABRIC
    );

    long jsiRuntime = Objects.requireNonNull(getReactApplicationContext().getJavaScriptContextHolder(), "[shadowlist] JavaScriptContextHolder is null").get();
    injectJSIBindings(jsiRuntime);

//    FabricUIManager fabricUIManager = (FabricUIManager)uiManager;
//    createCommitHook(fabricUIManager);
  }
}
