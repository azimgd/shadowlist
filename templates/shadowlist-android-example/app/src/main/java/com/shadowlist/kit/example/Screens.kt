package com.shadowlist.kit.example

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import com.shadowlist.kit.ShadowListKitListView
import com.shadowlist.kit.bench.ShadowListKitBench
import com.shadowlist.utils.ShadowListUtilsChatBubble
import com.shadowlist.utils.ShadowListUtilsChatLabels
import com.shadowlist.utils.ShadowListUtilsChatList
import com.shadowlist.utils.ShadowListUtilsChatMessage
import com.shadowlist.utils.ShadowListUtilsChatMessageStatus
import com.shadowlist.utils.ShadowListUtilsContactItem
import com.shadowlist.utils.ShadowListUtilsContactRow
import com.shadowlist.utils.ShadowListUtilsContactsSectionList
import com.shadowlist.utils.ShadowListUtilsFeedItem
import com.shadowlist.utils.ShadowListUtilsFeedLabels
import com.shadowlist.utils.ShadowListUtilsFeedList
import com.shadowlist.utils.ShadowListUtilsFeedRow
import com.shadowlist.utils.ShadowListUtilsImageLoader
import com.shadowlist.utils.ShadowListUtilsImageView
import com.shadowlist.utils.ShadowListUtilsList
import com.shadowlist.utils.ShadowListUtilsListFooter
import com.shadowlist.utils.ShadowListUtilsMasonryCard
import com.shadowlist.utils.ShadowListUtilsMasonryLabels
import com.shadowlist.utils.ShadowListUtilsMasonryList
import com.shadowlist.utils.ShadowListUtilsRow
import com.shadowlist.utils.ShadowListUtilsSectionHeader
import com.shadowlist.utils.ShadowListUtilsTheme
import com.shadowlist.utils.shadowListUtilsGroupIntoSections

/*
 * A screen of the example: one list on one engine with the example's data. On the sl engine the
 * list is the library's template for the screen, which takes items. The other engines take the
 * same items as the library's rows.
 */
abstract class ListScreen(
  val context: Context,
  val engine: Engine,
  inverted: Boolean = false,
  columns: Int = 1,
  makeTemplate: () -> ShadowListUtilsList,
) : Screen {
  val list = ListController(context, engine, inverted, columns, makeTemplate)
  override val view: View get() = list.backend.view

  /*
   * The theme of the rows built here, for the engines that are not the template.
   */
  protected val theme: ShadowListUtilsTheme = context.listTheme

  /*
   * Items generated so far. New ones continue the numbering.
   */
  protected var generated = 0

  abstract override fun load()
  open fun prependRows() {}
  open fun appendRows() {}

  protected fun <T> generate(count: Int, make: (Int) -> T): List<T> {
    val items = List(count) { make(generated + it) }
    generated += count
    return items
  }
}

class FeedScreen(context: Context, engine: Engine) : ListScreen(context, engine, makeTemplate = { ShadowListUtilsFeedList(context) }) {
  private val items = ArrayList<ShadowListUtilsFeedItem>()
  private val labels = ShadowListUtilsFeedLabels()

  private fun post(index: Int) = Items.feed(FeedPost(index), Settings.states)

  override fun load() {
    items.addAll(generate(Settings.count, ::post))
    show(RowChange.Reset)
  }

  override fun prependRows() {
    items.addAll(0, generate(10, ::post))
    show(RowChange.Prepend(10))
  }

  override fun appendRows() {
    items.addAll(generate(20, ::post))
    show(RowChange.Append(20))
  }

  private fun show(change: RowChange) {
    val feed = list.template as? ShadowListUtilsFeedList
    if (feed != null) list.update(feed) { feed.setItems(ArrayList(items), Settings.animate) }
    else list.setRows(items.map { ShadowListUtilsFeedRow(it, theme, labels) }, change)
  }
}

class ChatScreen(context: Context, engine: Engine) :
  ListScreen(context, engine, inverted = true, makeTemplate = { ShadowListUtilsChatList(context) }) {
  private val messages = ArrayList<ChatMessage>()
  private val labels = ShadowListUtilsChatLabels()

  /*
   * The keys of the messages that carry a caption.
   */
  private val captioned = HashSet<String>()

  private fun message(index: Int) = ChatMessage(index)

  private fun caption(message: ShadowListUtilsChatMessage): String? = if (message.key in captioned) "Edited" else null

  override fun load() {
    (list.template as? ShadowListUtilsChatList)?.captionForItem = ::caption
    messages.addAll(generate(Settings.count, ::message))
    show(RowChange.Reset)
  }

  override fun prependRows() {
    messages.addAll(0, generate(50, ::message))
    show(RowChange.Prepend(50))
  }

  override fun appendRows() {
    messages.addAll(generate(10, ::message))
    show(RowChange.Append(10))
  }

  /*
   * The messages as items. With SLStates every message whose index is 2 past a multiple of 5
   * carries a caption, the newest of the reader's own messages is on its way and the one before
   * it failed.
   */
  private fun items(): List<ShadowListUtilsChatMessage> {
    if (!Settings.states) return messages.map { Items.chat(it) }
    val own = messages.filter { it.isOwn }
    val sending = own.lastOrNull()
    val failed = own.getOrNull(own.size - 2)
    return messages.map { message ->
      if (message.index % CAPTION_EVERY == CAPTION_OFFSET) captioned.add(message.id)
      Items.chat(message, when {
        message === sending -> ShadowListUtilsChatMessageStatus.SENDING
        message === failed -> ShadowListUtilsChatMessageStatus.FAILED
        message.isOwn -> ShadowListUtilsChatMessageStatus.SENT
        else -> null
      })
    }
  }

  private fun show(change: RowChange) {
    val items = items()
    val chat = list.template as? ShadowListUtilsChatList
    if (chat != null) list.update(chat) { chat.setItems(items, Settings.animate) }
    else list.setRows(items.map { ShadowListUtilsChatBubble(it, caption(it), true, theme, labels) }, change)
  }

  private companion object {
    const val CAPTION_EVERY = 5
    const val CAPTION_OFFSET = 2
  }
}

class DirectoryScreen(context: Context, engine: Engine) :
  ListScreen(context, engine, makeTemplate = { ShadowListUtilsContactsSectionList(context) }) {
  private val contacts = ArrayList<ShadowListUtilsContactItem>()

  private fun contact(index: Int) = Items.contact(Contact(index))

  override fun load() {
    contacts.addAll(generate(Settings.count, ::contact))
    show(RowChange.Reset)
  }

  override fun prependRows() {
    contacts.addAll(0, generate(10, ::contact))
    show(RowChange.Update)
  }

  override fun appendRows() {
    contacts.addAll(generate(10, ::contact))
    show(RowChange.Update)
  }

  /*
   * Contacts grouped by first letter, each group under a sticky header and sorted by name. The
   * template takes the sections. The other engines take a header row before each group and pin
   * those rows.
   */
  private fun show(change: RowChange) {
    val sections = shadowListUtilsGroupIntoSections(contacts).map { section ->
      // A stable sort. Equal names keep their order.
      section.copy(items = section.items.sortedBy { it.name })
    }
    val footer = ShadowListUtilsListFooter(context, "${contacts.size} travellers", theme)
    val directory = list.template as? ShadowListUtilsContactsSectionList
    if (directory != null) {
      directory.footerView = footer
      list.update(directory) { directory.setSections(sections, Settings.animate) }
      return
    }
    val rows = ArrayList<ShadowListUtilsRow>()
    val headers = ArrayList<Int>()
    for (section in sections) {
      headers.add(rows.size)
      rows.add(ShadowListUtilsSectionHeader("section-${section.key}", section.title, section.items.size, theme))
      section.items.mapTo(rows) { ShadowListUtilsContactRow(it, false, theme) }
    }
    list.backend.setFooter(footer)
    list.backend.setStickyIndices(headers.toIntArray())
    list.setRows(rows, change)
  }
}

class GalleryScreen(context: Context, engine: Engine) :
  ListScreen(context, engine, columns = 3, makeTemplate = { ShadowListUtilsMasonryList(context) }) {
  private val labels = ShadowListUtilsMasonryLabels()

  override fun load() {
    val items = generate(Settings.count) { Items.photo(Photo(it)) }
    val gallery = list.template as? ShadowListUtilsMasonryList
    if (gallery != null) list.update(gallery) { gallery.items = items }
    else list.setRows(items.map { ShadowListUtilsMasonryCard(it, theme, labels) }, RowChange.Reset)
  }
}

/*
 * Routes by the names the iOS and React Native examples use.
 */
object Routes {
  val names = listOf("Feed", "Chat", "SectionList", "Masonry")

  /*
   * Feature screens, which always run on ShadowListKitListView.
   */
  val features = listOf(
    "Activity", "Poll", "Contacts", "Reorder", "ReorderGrid", "Snap", "Nested", "Inbox", "Horizontal", "Changes", "Collapsing", "Sections")

  fun make(name: String, context: Context, engine: Engine): Screen? = when (name) {
    "Feed" -> FeedScreen(context, engine)
    "Chat" -> ChatScreen(context, engine)
    "SectionList" -> DirectoryScreen(context, engine)
    "Masonry" -> GalleryScreen(context, engine)
    "Contacts" -> ContactsScreen(context)
    "Reorder" -> ReorderScreen(context)
    "ReorderGrid" -> ReorderGridScreen(context)
    "Snap" -> SnapScreen(context)
    "Nested" -> NestedScreen(context)
    "Activity" -> ActivityScreen(context)
    "Poll" -> PollScreen(context)
    "Inbox" -> InboxScreen(context)
    "Horizontal" -> HorizontalScreen(context, engine)
    "Changes" -> ChangesScreen(context, engine)
    "Collapsing" -> CollapsingScreen(context, engine)
    "Sections" -> SectionsScreen(context)
    else -> null
  }
}

/*
 * Opens the route from the launch extras, or a list of routes and engines.
 */
class MainActivity : Activity() {
  private var screen: Screen? = null

  override fun onCreate(savedInstanceState: Bundle?) {
    val night = (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES
    // The manifest's theme is the light one. A dark device gets the dark window and bar icons.
    if (night) setTheme(android.R.style.Theme_Material_NoActionBar)
    super.onCreate(savedInstanceState)
    Settings.read(intent)
    LaunchArgs.set(intent)
    if (!night) {
      // Light screens take dark status and navigation bar icons, or they vanish edge to edge.
      @Suppress("DEPRECATION")
      window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR or View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR
    }
    // SLImages 0: no image loads, for runs that measure the lists alone.
    if (!Settings.images) ShadowListUtilsImageLoader.shared = NoImages
    val route = Settings.route
    val screen = route?.let { Routes.make(it, this, Settings.engine) }
    if (screen == null) {
      setContentView(home())
      return
    }
    this.screen = screen
    window.decorView.setBackgroundColor(listTheme.colors.background)
    setContentView(insetFrame(screen.view))
    screen.load()
    ShadowListKitBench.startIfRequested(this, intent)
    if (screen is ListScreen) Scenario.startIfRequested(screen, intent)
    (screen.view as? ShadowListKitListView)?.let { AccessibilityCheck.runIfRequested(it) }
  }

  /*
   * The list below the status bar and above the navigation bar, on every engine alike.
   */
  private fun insetFrame(content: View): View {
    val frame = android.widget.FrameLayout(this)
    frame.setBackgroundColor(listTheme.colors.background)
    frame.addView(content)
    frame.setOnApplyWindowInsetsListener { view, insets ->
      @Suppress("DEPRECATION")
      view.setPadding(0, insets.systemWindowInsetTop, 0, insets.systemWindowInsetBottom)
      insets
    }
    return frame
  }

  private fun home(): View {
    val column = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
    for (engine in Engine.entries) {
      for (route in Routes.names) {
        column.addView(Button(this).apply {
          text = "$route · ${engine.title}"
          setOnClickListener { open(route, engine) }
        })
      }
    }
    for (route in Routes.features) {
      column.addView(Button(this).apply {
        text = "$route · ${Engine.SHADOWLIST.title}"
        setOnClickListener { open(route, Engine.SHADOWLIST) }
      })
    }
    return android.widget.ScrollView(this).apply { addView(column) }
  }

  private fun open(route: String, engine: Engine) {
    startActivity(Intent(this, MainActivity::class.java)
      .putExtra("SLRoute", route).putExtra("SLEngine", engine.id)
      .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK))
  }
}

/*
 * An image loader that loads nothing. Image views keep their placeholder color.
 */
private object NoImages : ShadowListUtilsImageLoader() {
  override fun load(url: String, into: ShadowListUtilsImageView) {}
  override fun cancel(view: ShadowListUtilsImageView) {}
}
