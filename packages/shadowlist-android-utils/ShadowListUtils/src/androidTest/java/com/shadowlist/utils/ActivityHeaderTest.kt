package com.shadowlist.utils

import android.widget.Button
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ActivityHeaderTest {
  @Test fun actions_wrap_and_keep_their_callbacks_and_accessible_labels() = onMain {
    var pressed = 0
    val actions = List(3) { index ->
      ShadowListUtilsActivityHeaderAction("Long action $index", { pressed++ }, accessibilityLabel = "Action $index")
    }
    val header = ShadowListUtilsActivityHeader(testContext, "Activity", "Your crew", actions, testTheme)
    header.measure(android.view.View.MeasureSpec.makeMeasureSpec(190f.px, android.view.View.MeasureSpec.EXACTLY),
      android.view.View.MeasureSpec.makeMeasureSpec(0, android.view.View.MeasureSpec.UNSPECIFIED))
    header.layout(0, 0, header.measuredWidth, header.measuredHeight)
    val first = header.getChildAt(1) as Button
    val second = header.getChildAt(2) as Button
    assertTrue(second.top > first.top)
    assertEquals("Action 0", first.contentDescription)
    first.performClick()
    assertEquals(1, pressed)
  }
}
