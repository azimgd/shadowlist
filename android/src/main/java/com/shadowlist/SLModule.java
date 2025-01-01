package com.shadowlist;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.module.annotations.ReactModule;

@ReactModule(name = SLModule.NAME)
public class SLModule extends NativeSLModuleSpec {

  public static final String NAME = "SLModule";

  public SLModule(ReactApplicationContext reactContext) {
    super(reactContext);
  }

  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public void setup() {
  }
}
