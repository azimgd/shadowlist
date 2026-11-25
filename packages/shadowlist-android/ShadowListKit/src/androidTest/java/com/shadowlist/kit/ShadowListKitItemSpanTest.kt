package com.shadowlist.kit

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ShadowListKitItemSpanTest {
  private open class Source :
    ShadowListKitListView.DataSource,
    ShadowListKitListView.Sizing {
    val keys = MutableList(200) { "span-$it" }
    val crosses = mutableMapOf<Int, Int>()
    override fun numberOfItems(listView: ShadowListKitListView) = keys.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int) = keys[index]
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("cell")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int {
      crosses[index] = crossSize
      return 40
    }
  }

  private class Sections :
    Source(),
    ShadowListKitListView.Sections {
    init {
      keys.clear()
      keys.addAll((0..<4).map { "section-item-$it" })
    }
    override fun numberOfSections(listView: ShadowListKitListView) = 3
    override fun numberOfItemsInSection(listView: ShadowListKitListView, section: Int) = if (section == 1) 0 else 2
    override fun titleForHeaderInSection(listView: ShadowListKitListView, section: Int) = "Header $section"
    override fun titleForFooterInSection(listView: ShadowListKitListView, section: Int) = if (section ==
      0
    ) {
      "Footer"
    } else {
      null
    }
    override fun sizeForHeaderInSection(listView: ShadowListKitListView, section: Int, crossSize: Int) = if (section ==
      1
    ) {
      0
    } else {
      10
    }
    override fun sizeForFooterInSection(listView: ShadowListKitListView, section: Int, crossSize: Int) = 0
  }

  private fun drain(list: ShadowListKitListView) {
    repeat(6) {
      list.requestLayout()
      list.measure(
        View.MeasureSpec.makeMeasureSpec(360, View.MeasureSpec.EXACTLY),
        View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY)
      )
      list.layout(0, 0, 360, 400)
    }
  }

  private fun list(source: Source): ShadowListKitListView {
    val context = InstrumentationRegistry.getInstrumentation().targetContext
    return ShadowListKitListView(context).apply {
      numberOfColumns = 3
      registerCell("cell") { ShadowListKitListCell(it, "cell") }
      dataSource = source
    }
  }

  @Test fun spansIncludeOffscreenRowsAndUseTheSameMeasurementWidth() {
    InstrumentationRegistry.getInstrumentation().runOnMainSync {
      val source = Source()
      val list = list(source)
      var calls = 0
      list.getItemSpan = { index ->
        calls++
        index % 3 + 1
      }
      list.reloadData()
      drain(list)
      assertEquals(200, calls)
      assertEquals(120f, list.rectForItem(0)!!.width(), 0.01f)
      assertEquals(240f, list.rectForItem(1)!!.width(), 0.01f)
      assertEquals(360f, list.rectForItem(2)!!.width(), 0.01f)
      assertEquals(40f, list.rectForItem(2)!!.top, 0.01f)
      assertEquals(360f, list.rectForItem(197)!!.width(), 0.01f)
      assertEquals(240, source.crosses[1])
      drain(list)
      assertEquals(200, calls)
    }
  }

  @Test fun columnChangesAndIndexDependentSpansUseTheCurrentSource() {
    InstrumentationRegistry.getInstrumentation().runOnMainSync {
      val source = Source()
      val list = list(source)
      list.getItemSpan = { if (it == 0) 99 else 2 }
      list.reloadData()
      drain(list)
      list.numberOfColumns = 2
      drain(list)
      assertEquals(360f, list.rectForItem(1)!!.width(), 0.01f)
      list.numberOfColumns = 4
      drain(list)
      assertEquals(360f, list.rectForItem(0)!!.width(), 0.01f)
      assertEquals(180f, list.rectForItem(1)!!.width(), 0.01f)
      val first = source.keys[0]
      source.keys[0] = source.keys[1]
      source.keys[1] = first
      list.reloadData()
      drain(list)
      assertEquals(360f, list.rectForItem(0)!!.width(), 0.01f)
      assertEquals(180f, list.rectForItem(1)!!.width(), 0.01f)
      list.getItemSpan = null
      drain(list)
      assertEquals(90f, list.rectForItem(0)!!.width(), 0.01f)
    }
  }

  @Test fun sectionEdgesSpanAllTracksWithoutCallingThePublicItemProvider() {
    InstrumentationRegistry.getInstrumentation().runOnMainSync {
      val source = Sections()
      val list = list(source)
      val indices = mutableListOf<Int>()
      list.getItemSpan = { index ->
        indices.add(index)
        index % 2 + 1
      }
      list.reloadData()
      drain(list)
      assertEquals(listOf(0, 1, 2, 3), indices) // Existing native item indices are global across sections.
      assertEquals(360f, list.rectForHeaderInSection(0)!!.width(), 0.01f)
      assertEquals(360f, list.rectForFooterInSection(0)!!.width(), 0.01f)
      assertEquals(50f, list.rectForHeaderInSection(1)!!.top, 0.01f)
      assertEquals(60f, list.rectForItem(2)!!.top, 0.01f)
      assertEquals(100, list.contentSize)
    }
  }
}
