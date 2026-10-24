package com.shadowlist;

import com.facebook.soloader.SoLoader;

/*
 * The row keys JS keeps in its list's native key store, read by the store's id and the version
 * the props name. See ShadowListStoreJsi.h. Accessibility counts rows and finds a row's index here
 * instead of in a rowKeys prop copied to Java on every change.
 */
final class ShadowListKeys {
  static {
    SoLoader.loadLibrary("react_codegen_ShadowListViewSpec");
  }

  private ShadowListKeys() {}

  /*
   * The number of rows, or -1 when the store or version is gone.
   */
  static int keyCount(long store, double version) {
    return nativeKeyCount(store, version);
  }

  /*
   * A key's row, or -1.
   */
  static int indexOfKey(long store, double version, String key) {
    return nativeIndexOfKey(store, version, key);
  }

  private static native int nativeKeyCount(long store, double version);

  private static native int nativeIndexOfKey(long store, double version, String key);
}
