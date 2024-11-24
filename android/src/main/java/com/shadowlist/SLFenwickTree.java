package com.shadowlist;

public class SLFenwickTree {
  static {
    System.loadLibrary("react_codegen_SLContainerSpec");
  }

  private long mNativePtr;

  public SLFenwickTree(float[] childrenMeasurements) {
    mNativePtr = nativeInit(childrenMeasurements);
  }

  private native long nativeInit(float[] childrenMeasurements);
  private native void nativeDestroy(long nativePtr);
  private native float nativeSum(long nativePtr, int index);
  private native float nativeLowerBound(long nativePtr, float offset);

  public float sum(int index) {
    return nativeSum(mNativePtr, index);
  }

  public float lowerBound(float offset) {
    return nativeLowerBound(mNativePtr, offset);
  }

  public void destroy() {
    nativeDestroy(mNativePtr);
    mNativePtr = 0;
  }
}
