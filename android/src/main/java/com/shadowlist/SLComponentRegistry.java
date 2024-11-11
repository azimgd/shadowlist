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
  private native void nativeRegisterComponent(long nativePtr, int componentId);
  private native void nativeUnregisterComponent(long nativePtr, int componentId);
  private native void nativeMountRange(long nativePtr, int visibleStartIndex, int visibleEndIndex);
  private native void nativeMount(long nativePtr, int[] indices);
  private native void nativeUnmount(long nativePtr, int[] indices);
  private native void nativeMountObserver(long nativePtr, SLObserver observer);
  private native void nativeUnmountObserver(long nativePtr, SLObserver observer);
  private native void nativeDestroy(long nativePtr);

  public interface SLObserver {
    void onVisibilityChanged(int id, boolean isVisible);
  }

  public void registerComponent(int componentId) {
    nativeRegisterComponent(mNativePtr, componentId);
  }

  public void unregisterComponent(int componentId) {
    nativeUnregisterComponent(mNativePtr, componentId);
  }

  public void mountRange(int visibleStartIndex, int visibleEndIndex) {
    nativeMountRange(mNativePtr, visibleStartIndex, visibleEndIndex);
  }

  public void mount(int[] indices) {
    nativeMount(mNativePtr, indices);
  }

  public void unmount(int[] indices) {
    nativeUnmount(mNativePtr, indices);
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
