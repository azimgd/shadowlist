package com.shadowlist.kit

import android.os.Parcel
import android.os.Parcelable

/*
 * A scroll position that survives data changes: the key of the item at the viewport start and
 * how far the viewport start is past that item's leading edge, in pixels.
 */
class ShadowListKitAnchorState(val key: String, val offset: Float) : Parcelable {
  override fun writeToParcel(parcel: Parcel, flags: Int) {
    parcel.writeString(key)
    parcel.writeFloat(offset)
  }

  override fun describeContents(): Int = 0

  override fun equals(other: Any?): Boolean = other is ShadowListKitAnchorState && other.key == key && other.offset == offset
  override fun hashCode(): Int = key.hashCode() * 31 + offset.hashCode()
  override fun toString(): String = "ShadowListKitAnchorState(key=$key, offset=$offset)"

  companion object CREATOR : Parcelable.Creator<ShadowListKitAnchorState> {
    override fun createFromParcel(parcel: Parcel): ShadowListKitAnchorState =
      ShadowListKitAnchorState(parcel.readString() ?: "", parcel.readFloat())

    override fun newArray(size: Int): Array<ShadowListKitAnchorState?> = arrayOfNulls(size)
  }
}
