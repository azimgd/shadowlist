package com.shadowlist.kit

import android.os.Parcel
import android.os.Parcelable

/*
 * A scroll position that survives data changes: the key of the row at the viewport start and
 * how far the viewport start is past that row's leading edge, in pixels.
 */
class SLKAnchorState(val key: String, val offset: Float) : Parcelable {
  override fun writeToParcel(parcel: Parcel, flags: Int) {
    parcel.writeString(key)
    parcel.writeFloat(offset)
  }

  override fun describeContents(): Int = 0

  override fun equals(other: Any?): Boolean = other is SLKAnchorState && other.key == key && other.offset == offset
  override fun hashCode(): Int = key.hashCode() * 31 + offset.hashCode()
  override fun toString(): String = "SLKAnchorState(key=$key, offset=$offset)"

  companion object CREATOR : Parcelable.Creator<SLKAnchorState> {
    override fun createFromParcel(parcel: Parcel): SLKAnchorState =
      SLKAnchorState(parcel.readString() ?: "", parcel.readFloat())

    override fun newArray(size: Int): Array<SLKAnchorState?> = arrayOfNulls(size)
  }
}
