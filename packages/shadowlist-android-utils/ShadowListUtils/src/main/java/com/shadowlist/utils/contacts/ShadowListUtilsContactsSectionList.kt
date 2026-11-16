package com.shadowlist.utils

import android.content.Context
import android.view.View
import com.shadowlist.kit.ShadowListKitSwipeActionsConfiguration

/*
 * Contacts in sections with headers that stick to the top. Each header shows the section's
 * title and how many contacts it holds. onPressItem, rowForItem, onDelete and
 * disclosureIndicator are those of ShadowListUtilsContactsList.
 */
class ShadowListUtilsContactsSectionList : ShadowListUtilsList {
  constructor(context: Context) : super(context)
  constructor(context: Context, theme: ShadowListUtilsTheme) : super(context, theme)

  /*
   * The groups. Setting them shows them without an animation.
   */
  var sections: List<ShadowListUtilsContactSection>
    get() = storedSections
    set(value) = setSections(value, false)

  /*
   * Show these sections. animated animates the change from the sections before.
   */
  fun setSections(sections: List<ShadowListUtilsContactSection>, animated: Boolean) {
    val items = sections.flatMap { it.items }
    val next = rows(items, storedItems, rows, { it.key }, ::makeRow)
    storedSections = sections
    storedItems = items
    showRows(next, animated)
  }

  /*
   * The contacts of every section in order. Setting them groups them with
   * shadowListUtilsGroupIntoSections.
   */
  var items: List<ShadowListUtilsContactItem>
    get() = storedItems
    set(value) = setItems(value, false)

  fun setItems(items: List<ShadowListUtilsContactItem>, animated: Boolean) =
    setSections(shadowListUtilsGroupIntoSections(items), animated)

  var onPressItem: ((ShadowListUtilsContactItem) -> Unit)? = null
    set(value) {
      field = value
      allowsSelection = value != null
      rebuildRows()
    }

  var rowForItem: ((ShadowListUtilsContactItem) -> ShadowListUtilsRow)? = null
    set(value) {
      field = value
      reloadRows()
    }

  var labels: ShadowListUtilsContactsLabels = ShadowListUtilsContactsLabels()

  /*
   * The titles of the index along the trailing edge, or null for none.
   */
  var sectionIndexTitles: List<String>? = null
    set(value) {
      field = value
      sectionAdapter.indexTitles = value
      reloadData()
    }

  var onDelete: ((ShadowListUtilsContactItem) -> Unit)? = null

  var disclosureIndicator: Boolean = true
    set(value) {
      if (field == value) return
      field = value
      rebuildRows()
    }

  private var storedSections: List<ShadowListUtilsContactSection> = emptyList()
  private var storedItems: List<ShadowListUtilsContactItem> = emptyList()

  init {
    stickySectionHeaders = true
    showsSeparators = true
    separatorInsetStartPx = theme.rowInset.px
    allowsSelection = false
  }

  override fun makeAdapter(): ShadowListUtilsListAdapter = ShadowListUtilsSectionListAdapter(this)

  private val sectionAdapter: ShadowListUtilsSectionListAdapter get() = adapter as ShadowListUtilsSectionListAdapter

  private fun makeRow(item: ShadowListUtilsContactItem): ShadowListUtilsRow =
    rowForItem?.invoke(item) ?: ShadowListUtilsContactRow(item, onPressItem != null && disclosureIndicator, theme)

  override fun makeRows(): List<ShadowListUtilsRow> = storedItems.map(::makeRow)

  override fun rebuildRows(animated: Boolean) = showRows(storedItems.map(::makeRow), animated)

  /*
   * A new theme or text size changes the headers' size too, which only a reload measures again.
   */
  override fun reloadRows() {
    super.reloadRows()
    reloadData()
    refreshHeaders()
  }

  /*
   * Hand the adapter the sections with their header rows, then show the rows of their items.
   */
  private fun showRows(rows: List<ShadowListUtilsRow>, animated: Boolean) {
    sectionAdapter.sections = storedSections.map { section ->
      val header = ShadowListUtilsSectionHeader(HEADER_KEY_PREFIX + section.key, section.title, section.items.size, theme)
      ShadowListUtilsListSection(section.key, header, section.items.size)
    }
    setRows(rows, animated)
    refreshHeaders()
  }

  /*
   * A header keeps its cell while its section stays, and the list configures only item cells
   * again. A shown header whose title, count or theme changed takes its new row here.
   */
  private fun refreshHeaders() {
    val headers = sectionAdapter.headerRows.associateBy { it.key }
    for (position in 0 until childCount) {
      val view = getChildAt(position) as? ShadowListUtilsSectionHeaderView ?: continue
      if (view.visibility != View.VISIBLE) continue
      val shown = view.row as? ShadowListUtilsSectionHeader ?: continue
      val next = headers[shown.key] as? ShadowListUtilsSectionHeader ?: continue
      if (next != shown) sectionAdapter.show(next, view)
    }
  }

  override fun didSelectItem(index: Int) {
    storedItems.getOrNull(index)?.let { onPressItem?.invoke(it) }
    deselectItem(index)
  }

  override fun trailingSwipeActions(index: Int): ShadowListKitSwipeActionsConfiguration? {
    val onDelete = onDelete ?: return null
    val item = storedItems.getOrNull(index) ?: return null
    return deletingSwipeActions(item, labels.delete, theme.colors.red, onDelete)
  }

  override fun accessibilityActions(index: Int): List<ShadowListUtilsAccessibilityAction> {
    val onDelete = onDelete ?: return emptyList()
    val item = storedItems.getOrNull(index) ?: return emptyList()
    return listOf(ShadowListUtilsAccessibilityAction(R.id.shadowlist_utils_action_delete, labels.delete) {
      onDelete(item)
      true
    })
  }

  private companion object {
    /*
     * Keeps a header row's key apart from the keys of the items, which share the layout cache.
     */
    const val HEADER_KEY_PREFIX = "ShadowListUtilsSectionHeader:"
  }
}
