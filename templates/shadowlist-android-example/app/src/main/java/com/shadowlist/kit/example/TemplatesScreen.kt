package com.shadowlist.kit.example

import android.content.Context
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import com.shadowlist.kit.ShadowListKitSwipeAction
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration
import com.shadowlist.utils.*
import kotlin.math.floor
import kotlin.math.roundToInt

/* Real public-component consumer. Callbacks change application state, never library models. */
class TemplatesScreen(private val context: Context) : Screen {
  private val theme = context.listTheme
  private val scroll = ScrollView(context)
  private val content = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL; setPadding(theme.grouped.inset.dp, 0, theme.grouped.inset.dp, theme.spacing.xxl.dp) }
  private val catalog = PixelGridColumn(context)
  private val status = TextView(context).apply { text = "Choose a suggestion"; setTextColor(theme.colors.label) }
  private val thinking = ShadowListUtilsAssistantThinking(context, "Compare the travel dates, available seats and the total fare before suggesting a route.", true, theme)
  private val tool = ShadowListUtilsAssistantToolCallCard(context, ShadowListUtilsAssistantToolCall("search", "Search fares", "Lisbon, three days", status = ShadowListUtilsAssistantToolStatus.Running), theme)
  private val typing = ShadowListUtilsAssistantTypingIndicator(context, theme)
  private val footer = ShadowListUtilsPollFooter(context, 3.0, theme)
  override val view: View get() = scroll
  override fun load() {
    scroll.setBackgroundColor(theme.colors.groupedBackground); scroll.addView(content)
    appendCatalog()
    append(ShadowListUtilsGroupedCaption(context, "Assistant", theme))
    append(status)
    val empty = ShadowListUtilsAssistantEmpty(context, theme)
    empty.suggestions = listOf(ShadowListUtilsAssistantSuggestion("Plan a trip", "Three days in Lisbon"), ShadowListUtilsAssistantSuggestion("Compare fares", "Find a quieter travel day"))
    empty.onSelectSuggestion = { status.text = "Selected: ${it.prompt}" }
    append(empty); append(thinking); append(tool); append(typing); append(footer)
    append(Button(context).apply {
      text = "Finish search and add a vote"
      setOnClickListener {
        thinking.active = false; thinking.thinkingMs = 1800.0
        tool.call = ShadowListUtilsAssistantToolCall("search", "Search fares", "Lisbon, three days", status = ShadowListUtilsAssistantToolStatus.Done("Three routes are available. The quieter option leaves on Tuesday."))
        typing.visibility = View.GONE; footer.totalVotes += 1
        status.text = "Search complete; vote total ${footer.totalVotes.toInt()}"
      }
    })
    val accountValue = ShadowListUtilsFormValue(context, "3 saved routes", theme)
    val save = ShadowListUtilsFormButton(context, "Save route", theme).apply {
      onPress = { accountValue.text = "4 saved routes"; status.text = "Route saved through Form.Button" }
    }
    val delete = ShadowListUtilsFormButton(context, "Remove saved route", theme).apply {
      destructive = true
      onPress = { accountValue.text = "3 saved routes"; status.text = "Saved route removed" }
    }
    append(ShadowListUtilsGroupedCaption(context, "Saved routes", theme))
    append(ShadowListUtilsFormCard(context, listOf(save, delete), theme))
    append(ShadowListUtilsGroupedCard(context, listOf(accountValue, ShadowListUtilsGroupedSeparator(context, theme = theme)), theme))
    append(ShadowListUtilsGroupedSection(context, "Travel account", listOf(ShadowListUtilsFormValue(context, "Lisbon · Tuesday", theme)), theme))
    val routeName = ShadowListUtilsFormInput(context, "Lisbon", "Route name", theme).apply {
      placeholder = "Route name"
      returnKeyType = ShadowListUtilsReturnKeyType.DONE
      onChange = { value = it }
      onSubmitEditing = { status.text = "Route submitted: $value" }
    }
    val budget = ShadowListUtilsFormInput(context, "24", "Budget", theme).apply {
      numeric = true
      onChange = { value = it }
    }
    append(ShadowListUtilsFormCard(context, listOf(
      ShadowListUtilsFormRow(context, "Route", listOf(routeName), theme),
      ShadowListUtilsFormRow(context, "Budget", listOf(budget), theme),
    ), theme))
    append(ShadowListUtilsSearchField(context, "", theme).apply {
      labels = ShadowListUtilsSearchFieldLabels("Filter routes")
      onChange = { value = it; status.text = "Filter: $it" }
    })
    append(ShadowListUtilsTextField(context, "", theme).apply {
      placeholder = "Travel notes"
      accessibilityLabel = "Travel notes"
      multiline = true
      onChange = { value = it }
    })
    val rippleTheme = theme.copy(pressFeedback = ShadowListUtilsThemePressFeedback.RIPPLE)
    val badge = ShadowListUtilsIndexBadge(context, 1, theme)
    val proceed = ShadowListUtilsPillButton(context, "Continue", rippleTheme)
    proceed.onPress = { proceed.disabled = true; status.text = "Route ready; add another route to continue again" }
    val add = ShadowListUtilsDashedButton(context, "Add route", theme).apply {
      onPress = { badge.index += 1; proceed.disabled = false; status.text = "Added route ${badge.index}" }
    }
    val glyph = TextView(context).apply { text = "⋯"; importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO; textSize = 22f; includeFontPadding = false; setTextColor(theme.colors.accent) }
    val more = ShadowListUtilsIconButton(context, glyph, "More routes", rippleTheme).apply {
      onPress = { status.text = "More routes requested" }
    }
    val buttons = LinearLayout(context).apply {
      orientation = LinearLayout.HORIZONTAL; gravity = android.view.Gravity.CENTER_VERTICAL
      addView(badge, LinearLayout.LayoutParams(22.dp, 22.dp))
      addView(proceed, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, 40.dp).apply { marginStart = 12.dp })
      addView(more, LinearLayout.LayoutParams(40.dp, 40.dp).apply { marginStart = 12.dp })
    }
    append(buttons); append(add)
    val direct = ShadowListUtilsChip(context, "Direct routes", theme).apply { isSelected = true; maxFontSizeMultiplier = 1.5f }
    val seat = ShadowListUtilsChip(context, "Seat 12A", theme).apply {
      variant = ShadowListUtilsChipVariant.PLAIN; size = ShadowListUtilsChipSize.COMPACT; mono = true
    }
    val train = ShadowListUtilsChoiceChip(context, "Train", theme.colors.fill, true, theme)
    val flight = ShadowListUtilsChoiceChip(context, "Flight", theme.colors.accentSoft, false, theme)
    val chips = ShadowListUtilsChipRow(context, listOf(direct, seat, train, flight), theme)
    direct.onPress = { direct.isSelected = !direct.isSelected; status.text = if (direct.isSelected) "Direct routes selected" else "Direct routes cleared" }
    direct.onClear = { chips.children = chips.children.filter { it !== direct }; status.text = "Direct routes removed" }
    seat.onClear = { chips.children = chips.children.filter { it !== seat }; status.text = "Seat preference removed" }
    train.onPress = { train.isSelected = true; flight.isSelected = false; status.text = "Train selected" }
    flight.onPress = { train.isSelected = false; flight.isSelected = true; status.text = "Flight selected" }
    append(chips)
    val segmented = ShadowListUtilsSegmented(context, listOf(
      ShadowListUtilsSegmentedOption("plan", "Plan"),
      ShadowListUtilsSegmentedOption("book", "Book"),
      ShadowListUtilsSegmentedOption("travel", "Travel"),
    ), "plan", theme).apply { accessibilityLabel = "Trip stage" }
    val bar = ShadowListUtilsProgressBar(context, 0.285, "Trip preparation progress", theme = theme)
    val ring = ShadowListUtilsProgressRing(context, 0.285, theme = theme)
    var requests = 0
    segmented.onChange = { id ->
      requests += 1; segmented.value = id
      bar.fraction = when (id) { "plan" -> 0.285; "book" -> 0.5; else -> 1.0 }
      ring.fraction = bar.fraction
      val label = segmented.options.firstOrNull { it.id == id }?.label ?: id
      status.text = "$label selected ($requests requests)"
    }
    append(ShadowListUtilsGroupedCaption(context, "Trip preparation", theme))
    append(segmented); append(bar)
    content.addView(ring, LinearLayout.LayoutParams(52.dp, 52.dp).apply { gravity = android.view.Gravity.CENTER_HORIZONTAL; bottomMargin = 16.dp })
    appendGroupedActions()
    val latest = ShadowListUtilsAssistantScrollButton(context, theme).apply {
      visible = true
      onPress = { scroll.smoothScrollTo(0, 0); status.text = "Returned to the first component" }
    }
    content.addView(latest, LinearLayout.LayoutParams(36.dp, 36.dp).apply { gravity = android.view.Gravity.CENTER_HORIZONTAL; bottomMargin = 16.dp })
  }

  private fun appendCatalog() {
    content.addView(catalog)
    fun row(views: List<View>, gap: Float = theme.spacing.sm) = LinearLayout(context).apply {
      gravity = android.view.Gravity.CENTER_VERTICAL
      views.forEachIndexed { index, child -> addView(child, LinearLayout.LayoutParams(-2, -2).apply {
        if (index > 0) marginStart = gap.dp
      }) }
    }
    fun section(title: String, views: List<View>, card: Boolean = false, gap: Float = theme.spacing.xxl) {
      val section = PixelGridColumn(context)
      section.addView(ShadowListUtilsGroupedCaption(context, title, theme).apply {
        setPadding(theme.grouped.rowInset.dp, 0, theme.grouped.rowInset.dp, 0)
      }, 0f, text = true)
      val body: View = if (card) LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        setPadding(theme.spacing.md.dp, theme.spacing.md.dp, theme.spacing.md.dp, theme.spacing.md.dp)
        background = android.graphics.drawable.GradientDrawable().apply {
          setColor(theme.colors.groupedCell); cornerRadius = theme.grouped.radius.dpf
        }
        views.forEachIndexed { index, child -> addView(child, LinearLayout.LayoutParams(-1, -2).apply {
          if (index > 0) topMargin = theme.spacing.md.dp
        }) }
      } else PixelGridColumn(context).apply {
        // A TextField is a text node in React Native, as a caption is.
        views.forEachIndexed { index, child -> addView(child, if (index == 0) 0f else theme.spacing.md, text = child is ShadowListUtilsTextField) }
      }
      section.addView(body, theme.spacing.sm - 2)
      catalog.addView(section, gap)
    }
    fun pill(label: String, style: ShadowListUtilsPillButtonVariant) = ShadowListUtilsPillButton(context, label, theme).apply {
      variant = style; onPress = { status.text = "$label selected" }
    }
    fun icon(drawable: android.graphics.drawable.Drawable) = android.widget.ImageView(context).apply {
      setImageDrawable(drawable); scaleType = android.widget.ImageView.ScaleType.CENTER
    }
    val wrap = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
    wrap.addView(row(listOf(pill("Save", ShadowListUtilsPillButtonVariant.PRIMARY), pill("Undo", ShadowListUtilsPillButtonVariant.TINTED), pill("Cancel", ShadowListUtilsPillButtonVariant.PLAIN), pill("Delete", ShadowListUtilsPillButtonVariant.DESTRUCTIVE))))
    wrap.addView(row(listOf(pill("Disabled", ShadowListUtilsPillButtonVariant.PRIMARY).apply { disabled = true },
      ShadowListUtilsIconButton(context, icon(ShadowListUtilsPlusIcon(theme.colors.accent, 19f)), "Add", theme).apply { onPress = { status.text = "Added" } })),
      LinearLayout.LayoutParams(-1, -2).apply { topMargin = theme.spacing.sm.dp })
    section("Buttons", listOf(wrap, ShadowListUtilsDashedButton(context, "Add sort", theme).apply { onPress = { status.text = "Sort added" } }))
    val fill = ShadowListUtilsChip(context, "Fill down", theme).apply { onPress = { status.text = "Filled down" } }
    val choices = listOf(
      ShadowListUtilsChoiceChip(context, "Planned", theme.colors.accentSoft, true, theme),
      ShadowListUtilsChoiceChip(context, "Booked", theme.colors.redSoft, false, theme),
      ShadowListUtilsChoiceChip(context, "Done", theme.colors.fill, false, theme))
    choices.forEach { choice -> choice.onPress = { choices.forEach { it.isSelected = it === choice } } }
    section("Chips", listOf(row(listOf(fill, ShadowListUtilsChip(context, "Clear", theme).apply { variant = ShadowListUtilsChipVariant.PLAIN; onPress = { fill.isSelected = false } })), row(choices)))
    section("Fields", listOf(
      ShadowListUtilsSearchField(context, "", theme).apply { onChange = { value = it } },
      ShadowListUtilsTextField(context, "", theme).apply { placeholder = "Gate code"; accessibilityLabel = "Gate code"; onChange = { value = it } },
      ShadowListUtilsTextField(context, "", theme).apply { placeholder = "Paste rows here"; accessibilityLabel = "Paste rows here"; multiline = true; onChange = { value = it } }))
    val periods = listOf(ShadowListUtilsSegmentedOption("day", "Day"), ShadowListUtilsSegmentedOption("week", "Week"), ShadowListUtilsSegmentedOption("month", "Month"))
    val period = ShadowListUtilsSegmented(context, periods, "week", theme)
    val formPeriod = ShadowListUtilsSegmented(context, periods, "week", theme).apply {
      layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
    }
    period.onChange = { period.value = it; formPeriod.value = it }
    formPeriod.onChange = { period.value = it; formPeriod.value = it }
    section("Segmented", listOf(period, ShadowListUtilsSegmented(context, listOf(ShadowListUtilsSegmentedOption("asc", "A→Z"), ShadowListUtilsSegmentedOption("desc", "Z→A")), "asc", theme).apply {
      size = ShadowListUtilsSegmentedSize.COMPACT; accessibilityLabel = "Order"; onChange = { value = it }
    }))
    val total = ShadowListUtilsFormValue(context, "51.00", theme)
    section("Form", listOf(ShadowListUtilsFormCard(context, listOf(
      ShadowListUtilsFormRow(context, "Name", listOf(ShadowListUtilsFormInput(context, "Boarding pass", "Name", theme).apply { onChange = { value = it } }), theme),
      ShadowListUtilsFormRow(context, "Price", listOf(ShadowListUtilsFormInput(context, "42.50", "Price", theme).apply {
        numeric = true; onChange = { value = it; total.text = it.toDoubleOrNull()?.let { price -> String.format(java.util.Locale.US, "%.2f", price * 1.2) } ?: "" }
      }), theme),
      ShadowListUtilsFormRow(context, "With tax", listOf(total), theme),
      ShadowListUtilsFormRow(context, "Period", listOf(formPeriod), theme).apply { stacked = true }), theme),
      ShadowListUtilsFormButton(context, "Remove item", theme).apply { destructive = true; onPress = { status.text = "Item removed" } }))
    val pressed = ShadowListUtilsGroupedRow(context, "Last press: Nothing yet", theme).apply { chevron = false }
    section("Grouped", listOf(ShadowListUtilsGroupedCard(context, listOf(
      ShadowListUtilsGroupedRow(context, "Trip files", theme).apply {
        subtitle = "12 documents"
        leading = icon(ShadowListUtilsFolderIcon(theme.colors.onAccent)).apply {
          layoutParams = LinearLayout.LayoutParams(30.dp, 30.dp)
          background = android.graphics.drawable.GradientDrawable().apply { setColor(theme.colors.accent); cornerRadius = 7.dpf }
        }
        onPress = { pressed.title = "Last press: Trip files" }
        onLongPress = { pressed.title = "Last press: Trip files, held" }; longPressLabel = "Show details"
      },
      ShadowListUtilsGroupedRow(context, "Currency", theme).apply {
        trailing = TextView(context).apply { text = "EUR"; theme.typography.body.applyTo(this, theme.colors.secondaryLabel) }
        separated = true; onPress = { pressed.title = "Last press: Currency" }
      },
      ShadowListUtilsGroupedRow(context, "Export", theme).apply {
        subtitle = "A long description that wraps onto a second line when it needs to"; subtitleLines = 2; chevron = false
        separated = true; onPress = { pressed.title = "Last press: Export" }
      },
      ShadowListUtilsGroupedRow(context, "Sync", theme).apply { subtitle = "Unavailable offline"; disabled = true; separated = true; onPress = {} },
      ShadowListUtilsGroupedSeparator(context, 0f, theme), pressed), theme).apply { setPadding(0, 0, 0, 0) }))
    // The grouped section's bottom padding joins the gap above the next caption.
    section("Progress", gap = theme.spacing.xl + theme.spacing.xxl, views = listOf(
      ShadowListUtilsProgressBar(context, 0.25, "25% saved", theme = theme),
      ShadowListUtilsProgressBar(context, 0.62, "62% saved", theme = theme).apply { color = theme.colors.green },
      ShadowListUtilsProgressBar(context, 0.9, "90% spent", theme = theme).apply { color = theme.colors.orange },
      ShadowListUtilsProgressBar(context, 1.2, "Over budget", height = 10f, theme = theme).apply { color = theme.colors.red },
      row(listOf(ShadowListUtilsProgressRing(context, 0.25, theme = theme), ShadowListUtilsProgressRing(context, 0.6, theme = theme), ShadowListUtilsProgressRing(context, 0.9, 40f, 5f, theme)))), card = true)
    section("Badges", listOf(row((1..3).map { ShadowListUtilsIndexBadge(context, it, theme) })))
    section("Empty state", listOf(ShadowListUtilsEmptyState(context, "No files yet", "Add one below.", icon(ShadowListUtilsDocIcon(theme.colors.tertiaryLabel, 28f)), theme)), card = true)
    val header = ShadowListUtilsSectionHeader("section-header", "Section header", 3, theme)
    section("Primitives", listOf(
      row(listOf(ShadowListUtilsAvatar(context, 40f, theme).apply { show("Ada Lovelace") }, ShadowListUtilsAvatar(context, 32f, theme).apply { show("Grace Hopper") }, ShadowListUtilsSpinner(context, theme = theme))),
      ShadowListUtilsListHeader(context, "List header", "With a subtitle", theme),
      header.makeView(context).apply { configure(header, null) },
      ShadowListUtilsItemSeparator(context, theme), ShadowListUtilsListFooter(context, "List footer", theme)), card = true)
    section("Assistant", listOf(row(listOf(
      ShadowListUtilsAssistantActionButton(context, "Copy", ShadowListUtilsCopyIcon(theme.colors.secondaryLabel, 18f), theme = theme, onPress = {}),
      ShadowListUtilsAssistantActionButton(context, "Retry", ShadowListUtilsRetryIcon(theme.colors.label, 18f), showLabel = true, theme = theme, onPress = {})))))
    val shelf = android.widget.HorizontalScrollView(context).apply {
      isHorizontalScrollBarEnabled = false
      addView(row(listOf("Lisbon", "12", "Kyoto", "7", "Reykjavík", "Oaxaca").map { label ->
        if (label.toIntOrNull() == null) ShadowListUtilsChip(context, label, theme).apply { onPress = {} }
        else TextView(context).apply {
          text = label; theme.typography.subhead.applyTo(this, theme.colors.label); fontFeatureSettings = "tnum"
          gravity = android.view.Gravity.CENTER; minWidth = 34.dp; minHeight = 34.dp
          setPadding(theme.spacing.sm.dp, 0, theme.spacing.sm.dp, 0)
          background = android.graphics.drawable.GradientDrawable().apply { setColor(theme.colors.elevated2); cornerRadius = 17.dpf }
        }
      }))
    }
    section("Template list", listOf(shelf))
    content.addView(View(context), LinearLayout.LayoutParams(1, theme.spacing.xxl.dp))
  }

  private fun appendGroupedActions() {
    append(ShadowListUtilsGroupedCaption(context, "Reservation actions", theme))
    val row = ShadowListUtilsGroupedRow(context, "Reservation", theme).apply {
      subtitle = "Kyoto, Tuesday"
      accessibilityRole = ShadowListUtilsGroupedRowAccessibilityRole.LINK
      testID = "reservation-row"
      leading = Button(context).apply { text = "Map"; setOnClickListener { status.text = "Map opened" } }
      trailing = TextView(context).apply { text = "2 nights"; setTextColor(theme.colors.secondaryLabel) }
      onPress = { status.text = "Reservation opened" }
      onLongPress = { status.text = "Reservation details" }
      longPressLabel = "Reservation details"
      trailingSwipeActions = ShadowListKitSwipeActionsConfiguration(listOf(
        ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.DESTRUCTIVE, "Remove") { _, done ->
          title = "Reservation removed"; status.text = "Reservation removed"; done(true)
        })).apply { performsFirstActionWithFullSwipe = false }
      contextMenu = { menu ->
        menu.add("Inspect reservation").setOnMenuItemClickListener { status.text = "Reservation menu inspected"; true }
        true
      }
    }
    val disabled = ShadowListUtilsGroupedRow(context, "Read-only reservation", theme).apply {
      subtitle = "Actions remain available"; this.disabled = true; separated = true
      onPress = { status.text = "Unexpected disabled primary press" }
      leadingSwipeActions = ShadowListKitSwipeActionsConfiguration(listOf(
        ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Save") { _, done -> status.text = "Read-only reservation saved"; done(true) }))
      contextMenu = { menu ->
        menu.add("Inspect read-only reservation").setOnMenuItemClickListener { status.text = "Read-only menu inspected"; true }
        true
      }
    }
    append(ShadowListUtilsGroupedCard(context, listOf(row, disabled), theme))
    val booking = Button(context).apply { text = "Swipe this booking"; setOnClickListener { status.text = "Booking opened" } }
    val standalone = ShadowListUtilsGroupedActions(context, listOf(booking), theme).apply {
      trailingSwipeActions = ShadowListKitSwipeActionsConfiguration(listOf(
        ShadowListKitSwipeAction(ShadowListKitSwipeAction.Style.NORMAL, "Save booking") { _, done -> status.text = "Booking saved"; done(true) }))
      contextMenu = { menu -> menu.add("Inspect booking").setOnMenuItemClickListener { status.text = "Booking menu inspected"; true }; true }
    }
    append(standalone)
  }
  private fun append(child: View) {
    content.addView(child, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { bottomMargin = 16.dp })
  }
}

/*
 * A column that places its children the way React Native does. It adds the gaps and heights
 * before it rounds a child's top to a pixel, and floors the top of a text node. A LinearLayout
 * rounds every gap by itself and the error grows down the page.
 */
class PixelGridColumn(context: Context) : android.view.ViewGroup(context) {
  private class Params(val gap: Float, val text: Boolean) : LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT) { var top = 0 }

  /*
   * The column's height before rounding, for a column that holds this one.
   */
  var exactHeight = 0f
    private set

  init {
    // A child placed from a rounded top can end a pixel below the column's rounded height.
    clipChildren = false
  }

  /*
   * Adds a child a gap in dp below the one before it.
   */
  fun addView(child: View, gap: Float, text: Boolean = false) = addView(child, Params(gap, text))

  override fun checkLayoutParams(params: LayoutParams?): Boolean = params is Params
  override fun generateDefaultLayoutParams(): LayoutParams = Params(0f, false)
  override fun generateLayoutParams(params: LayoutParams?): LayoutParams = Params(0f, false)

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val width = MeasureSpec.getSize(widthMeasureSpec)
    var bottom = 0f
    for (index in 0 until childCount) {
      val child = getChildAt(index)
      val params = child.layoutParams as Params
      child.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED))
      val top = bottom + params.gap.dpf
      params.top = if (params.text) floor(top).toInt() else top.roundToInt()
      bottom = top + exactHeight(child)
    }
    exactHeight = bottom
    setMeasuredDimension(width, bottom.roundToInt())
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    for (index in 0 until childCount) {
      val child = getChildAt(index)
      val childTop = (child.layoutParams as Params).top
      child.layout(0, childTop, child.measuredWidth, childTop + child.measuredHeight)
    }
  }

  /*
   * A control is a whole number of dp high and rounds that to pixels. Such a height counts as
   * the dp it came from.
   */
  private fun exactHeight(child: View): Float {
    if (child is PixelGridColumn) return child.exactHeight
    val dp = (child.measuredHeight / resources.displayMetrics.density).roundToInt()
    return if (dp.dp == child.measuredHeight) dp.dpf else child.measuredHeight.toFloat()
  }
}
