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
  private native int nativeLowerBound(long nativePtr, float offset);
  private native int nativeAdjustVisibleStartIndex(long nativePtr, int visibleStartIndex, int childrenMeasurementsTreeSize);
  private native int nativeAdjustVisibleEndIndex(long nativePtr, int visibleEndIndex, int childrenMeasurementsTreeSize);

  public float sum(int index) {
    return nativeSum(mNativePtr, index);
  }

  public int lowerBound(float offset) {
    return nativeLowerBound(mNativePtr, offset);
  }

  public int adjustVisibleStartIndex(int visibleStartIndex, int childrenMeasurementsTreeSize) {
    return nativeAdjustVisibleStartIndex(mNativePtr, visibleStartIndex, childrenMeasurementsTreeSize);
  }

  public int adjustVisibleEndIndex(int visibleStartIndex, int childrenMeasurementsTreeSize) {
    return nativeAdjustVisibleEndIndex(mNativePtr, visibleStartIndex, childrenMeasurementsTreeSize);
  }

  public void destroy() {
    nativeDestroy(mNativePtr);
    mNativePtr = 0;
  }
}
