package com.shadowlist.kit.example

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import com.shadowlist.kit.ShadowListKitAnchorState
import com.shadowlist.kit.ShadowListKitListCell
import com.shadowlist.kit.ShadowListKitListView
import com.shadowlist.kit.ShadowListKitTextLayout
import com.shadowlist.kit.ShadowListKitTextView
import com.shadowlist.kit.ShadowListKitViewabilityConfig
import com.shadowlist.kit.ShadowListKitViewabilityConfigCallbackPair
import com.shadowlist.kit.ShadowListKitViewableItemsChangedInfo
import com.shadowlist.utils.ShadowListUtilsLayoutCache
import com.shadowlist.utils.ShadowListUtilsRow
import com.shadowlist.utils.ShadowListUtilsRowLayout
import com.shadowlist.utils.ShadowListUtilsRowView
import com.shadowlist.utils.ShadowListUtilsSegmented
import com.shadowlist.utils.ShadowListUtilsSegmentedOption
import com.shadowlist.utils.ShadowListUtilsSegmentedSize
import com.shadowlist.utils.ShadowListUtilsTextField
import com.shadowlist.utils.ShadowListUtilsTheme
import kotlin.math.floor
import kotlin.math.roundToInt

/*
 * A row of the lab: a title and one to four lines of filler. Every seventh or fifth row is
 * marked. In the spans layout the filler is not cut to its lines.
 */
class LabRow(val index: Int, val marked: Boolean, val truncates: Boolean) : ShadowListUtilsRow {
  val lines = 1 + (index * 7) % 4

  override val key: String get() = "lab-$index"
  override val contentVersion: Int get() = (if (marked) 1 else 0) or (if (truncates) 2 else 0)
  override val reuseIdentifier: String get() = REUSE_IDENTIFIER
  override fun makeView(context: Context): ShadowListUtilsRowView = LabRowView(context)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout {
    val spacing = theme.spacing
    val textWidth = (widthPx - 2 * spacing.lg.dp).coerceAtLeast(1)
    val title = theme.typography.headline.layout("Row $index", textWidth)
    val text = theme.typography.subhead.layout(List(lines) { FILLER }.joinToString(" "), textWidth, if (truncates) lines else 0)
    // React Native floors a text origin to the pixel grid and rounds the vertical padding once.
    val titleTop = floor(spacing.md.dpf).toInt()
    return LabRowLayout(widthPx, title.height + text.height + (2 * spacing.md).dp, title, titleTop, text, titleTop + title.height)
  }

  companion object {
    const val REUSE_IDENTIFIER = "lab"
    private const val FILLER = "Rows have different heights so scroll commands land through estimated sizes."
    private val theme = ShadowListUtilsTheme.LIGHT
  }
}

class LabRowLayout(
  widthPx: Int, heightPx: Int,
  val title: ShadowListKitTextLayout, val titleTop: Int,
  val text: ShadowListKitTextLayout, val textTop: Int,
) : ShadowListUtilsRowLayout(widthPx, heightPx)

class LabRowView(context: Context) : ShadowListUtilsRowView(context, LabRow.REUSE_IDENTIFIER) {
  private val theme = context.listTheme
  private val title = ShadowListKitTextView(context)
  private val text = ShadowListKitTextView(context)
  private val shape = GradientDrawable().apply { cornerRadius = theme.radius.sm.dpf }
  private var marked = false

  init {
    title.textColor = theme.colors.label
    text.textColor = theme.colors.secondaryLabel
    addView(title)
    addView(text)
  }

  override fun fill(row: ShadowListUtilsRow) {
    val lab = row as LabRow
    marked = lab.marked
    contentDescription = "Row ${lab.index}"
    showBackground()
  }

  override fun apply(row: ShadowListUtilsRow, layout: ShadowListUtilsRowLayout) {
    val frames = layout as LabRowLayout
    val inset = theme.spacing.lg.dp
    title.textLayout = frames.title
    title.place(inset, frames.titleTop, frames.title.width, frames.title.height)
    text.textLayout = frames.text
    text.place(inset, frames.textTop, frames.text.width, frames.text.height)
  }

  override fun setHighlighted(highlighted: Boolean, animated: Boolean) {
    super.setHighlighted(highlighted, animated)
    showBackground()
  }

  private fun showBackground() {
    val colors = theme.colors
    shape.setColor(if (highlighted) colors.fill else if (marked) colors.accentSoft else colors.background)
    background = shape
  }
}

/*
 * The list's scroll commands, scroll events and viewability in one place. It opens at row 120.
 * The More menu jumps, saves and restores the position and switches layouts. Two viewability
 * configs report at once.
 */
class ScrollLabScreen(context: Context) : Screen, ShadowListKitListView.DataSource, ShadowListKitListView.Sizing,
  ShadowListKitListView.ContentVersions, ShadowListKitListView.Delegate {
  private val theme = context.listTheme
  private val list = ShadowListKitListView(context)
  private val layouts = ShadowListUtilsLayoutCache()
  private var rows: List<LabRow> = emptyList()
  private val page = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
  private val status = TextView(context)
  private val header = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
  private val layout = ShadowListUtilsSegmented(context, listOf(
    ShadowListUtilsSegmentedOption("1", "List"),
    ShadowListUtilsSegmentedOption("2", "Grid"),
    ShadowListUtilsSegmentedOption("3", "Spans"),
  ), "1", theme)
  private val padding = ShadowListUtilsSegmented(context, listOf(
    ShadowListUtilsSegmentedOption("none", "No Padding"),
    ShadowListUtilsSegmentedOption("padded", "Padding"),
  ), "none", theme)
  private val query = ShadowListUtilsTextField(context, "", theme)
  override val view: View get() = page

  private var columns = 1
  private var marked = 7
  private var anchor: ShadowListKitAnchorState? = null
  private var positioned = false
  private var momentumEndCount = 0
  private var pendingViewOffset = 0

  private var scrollLine = "—"
  private var phaseLine = "—"
  private var viewableLine = "—"
  private var coveredLine = "—"
  private var commandLine = "—"
  private var pressedLine = "—"

  override fun load() {
    page.setBackgroundColor(theme.colors.background)
    list.setBackgroundColor(theme.colors.background)
    list.clipToPadding = false
    list.estimatedItemSizePx = 120.dpf
    list.separatorInsetStartPx = theme.rowInset.dp
    list.separatorColor = theme.colors.separator
    list.registerCell(LabRow.REUSE_IDENTIFIER) { LabRowView(it) }
    list.viewabilityConfigCallbackPairs = viewabilityPairs()
    list.dataSource = this
    list.delegate = this
    installHeader()
    if (Settings.debug) installStatus()
    page.addView(list, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
    list.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ -> openAtInitialIndex() }
    applyLayout()
  }

  // region Header

  private fun installHeader() {
    val spacing = theme.spacing
    header.setBackgroundColor(theme.colors.groupedBackground)
    header.setPadding(spacing.lg.dp, spacing.lg.dp, spacing.lg.dp, spacing.lg.dp)
    layout.size = ShadowListUtilsSegmentedSize.COMPACT
    layout.accessibilityLabel = "Layout"
    layout.onChange = { id ->
      layout.value = id
      columns = id.toIntOrNull() ?: 1
      applyLayout()
    }
    padding.size = ShadowListUtilsSegmentedSize.COMPACT
    padding.accessibilityLabel = "Content padding"
    padding.onChange = { id ->
      padding.value = id
      if (id == "padded") list.setPadding(PADDED_HORIZONTAL_DP.dp, PADDED_TOP_DP.dp, PADDED_HORIZONTAL_DP.dp, PADDED_BOTTOM_DP.dp)
      else list.setPadding(0, 0, 0, 0)
    }
    query.placeholder = "Type, then tap a row"
    query.onChange = { query.value = it }
    listOf(layout, padding, query).forEachIndexed { index, view ->
      header.addView(view, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
        if (index > 0) topMargin = spacing.sm.dp
      })
    }
    list.headerView = header
  }

  // endregion

  // region Status

  private fun installStatus() {
    val spacing = theme.spacing
    val container = FrameLayout(view.context)
    container.setBackgroundColor(theme.colors.groupedBackground)
    container.setPadding(spacing.lg.dp, spacing.xs.dp, spacing.lg.dp, spacing.xs.dp)
    container.contentDescription = "lab-status"
    theme.typography.caption.applyTo(status, theme.colors.secondaryLabel)
    status.setLines(STATUS_LINES)
    status.setHorizontallyScrolling(true)
    container.addView(status)
    page.addView(container, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
    showStatus()
  }

  private fun showStatus() {
    if (status.parent == null) return
    status.text = listOf(
      "scroll $scrollLine",
      "phase $phaseLine size ${(list.width / 1.dpf).roundToInt()}x${(list.contentSize / 1.dpf).roundToInt()}",
      "viewable $viewableLine covered $coveredLine",
      "command $commandLine pressed $pressedLine",
    ).joinToString("\n")
  }

  private fun describeScroll(name: String): String =
    "$name y=${(list.scrollY / 1.dpf).roundToInt()}/${(list.contentSize / 1.dpf).roundToInt()} h=${(list.height / 1.dpf).roundToInt()}"

  // endregion

  // region Layout

  /*
   * The rows for the layout and the marked rows. A new layout opens at the initial row again.
   */
  private fun applyLayout() {
    val reopens = list.numberOfColumns != columns
    list.numberOfColumns = columns
    list.showsSeparators = columns == 1
    list.getItemSpan = if (columns == 3) { index -> if (index % 17 == 0) 3 else if (index % marked == 0) 2 else 1 } else null
    rows = List(ROW_COUNT) { LabRow(it, it % marked == 0, columns != 3) }
    if (reopens || !positioned) {
      list.reloadData()
      positioned = false
      list.requestLayout()
    } else {
      list.applyChanges()
    }
  }

  private fun openAtInitialIndex() {
    if (positioned || list.height <= 0) return
    positioned = true
    // The command runs after the layout pass that gave the list its size.
    list.post { list.scrollToItem(INITIAL_INDEX, 0.0, false) }
  }

  // endregion

  // region Actions

  override fun headerMenu(): List<List<HeaderAction>> {
    fun action(label: String, checked: Boolean? = null, work: () -> Unit) = HeaderAction(label, checked) {
      work()
      showStatus()
    }
    return listOf(
      listOf(
        action("Scroll to Row 300 (Top)") { scrollTo(300, 0.0) },
        action("Scroll to Row 40 (Center)") { scrollTo(40, 0.5) },
        action("Scroll to Row 450 (Bottom, 20 pt)") { scrollTo(450, 1.0, 20) },
        action("Scroll to Row 9999") { scrollTo(9999, 0.0) },
      ),
      listOf(
        action("Scroll to Start") {
          commandLine = "offset 0"
          list.scrollToStart(true)
        },
        action("Scroll to End") {
          commandLine = "end"
          list.scrollToEnd(true)
        },
        action("Flash Scroll Indicators") { commandLine = "flash" },
      ),
      listOf(
        action("Save Position") {
          anchor = list.anchorState
          commandLine = anchor?.let { "saved ${it.key}@${"%.1f".format(it.offset)}" } ?: "saved none"
        },
        action("Restore Position") {
          anchor?.let {
            commandLine = "restore ${it.key}@${"%.1f".format(it.offset)}"
            list.restoreAnchorState(it)
          }
        },
      ),
      listOf(
        action("Scrolling Enabled", checked = list.scrollEnabled) { list.scrollEnabled = !list.scrollEnabled },
        action("Mark Every ${if (marked == 7) 5 else 7}th Row") {
          marked = if (marked == 7) 5 else 7
          applyLayout()
        },
      ),
    )
  }

  /*
   * Scroll to a row. The kit places a row by viewPosition alone, and viewOffset moves the list
   * on from there once it rests. A row the data lacks reports the failure and does nothing.
   */
  private fun scrollTo(index: Int, viewPosition: Double, viewOffset: Int = 0) {
    if (index !in rows.indices) {
      commandLine = "failed $index count=${rows.size}"
      return
    }
    commandLine = "to $index pos=$viewPosition off=$viewOffset"
    pendingViewOffset = viewOffset
    list.scrollToItem(index, viewPosition, true)
  }

  // endregion

  // region Viewability

  /*
   * Two viewability configs at once: rows at least half visible, and rows covering a tenth of
   * the viewport for 250 ms after the first interaction.
   */
  private fun viewabilityPairs(): List<ShadowListKitViewabilityConfigCallbackPair> {
    fun range(info: ShadowListKitViewableItemsChangedInfo): String? {
      val indices = info.viewableItems.map { it.index }
      return if (indices.isEmpty()) null else "${indices.min()}–${indices.max()}"
    }
    return listOf(
      ShadowListKitViewabilityConfigCallbackPair(ShadowListKitViewabilityConfig(itemVisiblePercentThreshold = 50.0)) { info ->
        viewableLine = range(info)?.let { "$it (+${info.changed.count { token -> token.isViewable }})" } ?: "—"
        showStatus()
      },
      ShadowListKitViewabilityConfigCallbackPair(
        ShadowListKitViewabilityConfig(viewAreaCoveragePercentThreshold = 10.0, minimumViewTime = 250.0, waitForInteraction = true),
      ) { info ->
        coveredLine = range(info) ?: "—"
        showStatus()
      },
    )
  }

  // endregion

  // region Data

  override fun numberOfItems(listView: ShadowListKitListView) = rows.size
  override fun keyForItem(listView: ShadowListKitListView, index: Int) = rows[index].key
  override fun contentVersionForItem(listView: ShadowListKitListView, index: Int) = rows[index].contentVersion.toLong()

  override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
    listView.dequeueReusableCell<LabRowView>(LabRow.REUSE_IDENTIFIER).also { it.configure(rows[index], null) }

  override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int =
    layouts.layout(rows[index], crossSize).heightPx + (if (listView.showsSeparators) listView.separatorThicknessPx else 0)

  // endregion

  // region Events

  override fun didSelectItem(listView: ShadowListKitListView, index: Int) {
    listView.deselectItem(index, false)
    pressedLine = "row $index"
    showStatus()
  }

  override fun didScroll(listView: ShadowListKitListView) {
    scrollLine = describeScroll("")
    showStatus()
  }

  override fun willBeginDragging(listView: ShadowListKitListView) {
    pendingViewOffset = 0
    phaseLine = describeScroll("drag")
    showStatus()
  }

  override fun didEndDragging(listView: ShadowListKitListView, willDecelerate: Boolean) {
    phaseLine = describeScroll(if (willDecelerate) "glide" else "release")
    showStatus()
  }

  override fun didEndDecelerating(listView: ShadowListKitListView) {
    momentumEndCount += 1
    phaseLine = "${describeScroll("rest")} end=$momentumEndCount"
    showStatus()
  }

  override fun didEndScrollingAnimation(listView: ShadowListKitListView) {
    if (pendingViewOffset == 0) return
    val offset = pendingViewOffset
    pendingViewOffset = 0
    listView.adjustOffsetBy(-offset.dpf.toDouble())
  }

  // endregion

  private companion object {
    const val ROW_COUNT = 500
    const val INITIAL_INDEX = 120
    const val STATUS_LINES = 4
    const val PADDED_TOP_DP = 24
    const val PADDED_BOTTOM_DP = 48
    const val PADDED_HORIZONTAL_DP = 16
  }
}
