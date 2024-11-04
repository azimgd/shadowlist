package com.shadowlist;

public class SLComponentRegistry {
  static {
    System.loadLibrary("react_codegen_SLContainerSpec");
  }

  private long nativePtr;

  public SLComponentRegistry() {
    nativePtr = nativeInit();
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
    nativeRegisterComponent(nativePtr, componentId);
  }

  public void unregisterComponent(int componentId) {
    nativeUnregisterComponent(nativePtr, componentId);
  }

  public void mountRange(int visibleStartIndex, int visibleEndIndex) {
    nativeMountRange(nativePtr, visibleStartIndex, visibleEndIndex);
  }

  public void mount(int[] indices) {
    nativeMount(nativePtr, indices);
  }

  public void unmount(int[] indices) {
    nativeUnmount(nativePtr, indices);
  }

  public void mountObserver(SLObserver observer) {
    nativeMountObserver(nativePtr, observer);
  }

  public void unmountObserver(SLObserver observer) {
    nativeUnmountObserver(nativePtr, observer);
  }

  public void destroy() {
    nativeDestroy(nativePtr);
    nativePtr = 0;
  }
}
