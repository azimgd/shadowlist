package com.shadowlist;

public class SLComponentRegistry {
  static {
    System.loadLibrary("react_codegen_SLContainerSpec");
  }

  private long mNativePtr;

  public SLComponentRegistry() {
    mNativePtr = nativeInit();
  }

  private native long nativeInit();
  private native void nativeRegisterComponent(long nativePtr, String uniqueId);
  private native void nativeUnregisterComponent(long nativePtr, String uniqueId);
  private native void nativeMount(long nativePtr, String[] uniqueIds);
  private native void nativeUnmount(long nativePtr, String[] uniqueIds);
  private native void nativeMountObserver(long nativePtr, SLObserver observer);
  private native void nativeUnmountObserver(long nativePtr, SLObserver observer);
  private native void nativeDestroy(long nativePtr);

  public interface SLObserver {
    void onVisibilityChanged(String uniqueId, boolean isVisible);
  }

  public void registerComponent(String uniqueId) {
    nativeRegisterComponent(mNativePtr, uniqueId);
  }

  public void unregisterComponent(String uniqueId) {
    nativeUnregisterComponent(mNativePtr, uniqueId);
  }

  public void mount(String[] uniqueIds) {
    nativeMount(mNativePtr, uniqueIds);
  }

  public void unmount(String[] uniqueIds) {
    nativeUnmount(mNativePtr, uniqueIds);
  }

  public void mountObserver(SLObserver observer) {
    nativeMountObserver(mNativePtr, observer);
  }

  public void unmountObserver(SLObserver observer) {
    nativeUnmountObserver(mNativePtr, observer);
  }

  public void destroy() {
    nativeDestroy(mNativePtr);
    mNativePtr = 0;
  }
}
