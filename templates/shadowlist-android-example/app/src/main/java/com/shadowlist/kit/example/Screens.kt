package com.shadowlist.kit.example

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import com.shadowlist.kit.ShadowListKitListView
import com.shadowlist.kit.bench.ShadowListKitBench

/*
 * A screen of the example: one list on one engine with the example's data.
 */
abstract class ListScreen(val context: Context, val engine: Engine, inverted: Boolean = false, columns: Int = 1) : Screen {
  val list = ListController(context, engine, inverted, columns)
  override val view: View get() = list.backend.view

  // Items generated so far. New ones continue the numbering.
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

class FeedScreen(context: Context, engine: Engine) : ListScreen(context, engine) {
  private val posts = ArrayList<FeedPost>()

  override fun load() {
    posts.addAll(generate(Settings.count, ::FeedPost))
    show(RowChange.Reset)
  }

  override fun prependRows() {
    posts.addAll(0, generate(10, ::FeedPost))
    show(RowChange.Prepend(10))
  }

  override fun appendRows() {
    posts.addAll(generate(20, ::FeedPost))
    show(RowChange.Append(20))
  }

  private fun show(change: RowChange) = list.setRows(posts.map { FeedRow(it) }, change)
}

class ChatScreen(context: Context, engine: Engine) : ListScreen(context, engine, inverted = true) {
  private val messages = ArrayList<ChatMessage>()

  override fun load() {
    messages.addAll(generate(Settings.count, ::ChatMessage))
    show(RowChange.Reset)
  }

  override fun prependRows() {
    messages.addAll(0, generate(50, ::ChatMessage))
    show(RowChange.Prepend(50))
  }

  override fun appendRows() {
    messages.addAll(generate(10, ::ChatMessage))
    show(RowChange.Append(10))
  }

  private fun show(change: RowChange) = list.setRows(messages.map { ChatRow(it) }, change)
}

class DirectoryScreen(context: Context, engine: Engine) : ListScreen(context, engine) {
  private val contacts = ArrayList<Contact>()

  override fun load() {
    contacts.addAll(generate(Settings.count, ::Contact))
    show(RowChange.Reset)
  }

  override fun prependRows() {
    contacts.addAll(0, generate(10, ::Contact))
    show(RowChange.Update)
  }

  override fun appendRows() {
    contacts.addAll(generate(10, ::Contact))
    show(RowChange.Update)
  }

  /*
   * Contacts grouped by first letter, each group under a sticky header.
   */
  private fun show(change: RowChange) {
    val groups = contacts.groupBy { it.author.name.firstOrNull()?.uppercase() ?: "#" }.toSortedMap()
    val rows = ArrayList<Row>()
    val headers = ArrayList<Int>()
    for ((title, group) in groups) {
      // A stable sort. Equal names keep their order.
      val members = group.sortedBy { it.author.name }
      headers.add(rows.size)
      // The header's count changes under the same key.
      rows.add(SectionHeaderRow(title, members.size).also { list.layouts.invalidate(it.key) })
      members.forEachIndexed { position, contact -> rows.add(ContactRow(contact, position < members.size - 1)) }
    }
    list.backend.setFooter(ListFooterView(context, "${contacts.size} travellers"))
    list.backend.setStickyIndices(headers.toIntArray())
    list.setRows(rows, change)
  }
}

class GalleryScreen(context: Context, engine: Engine) : ListScreen(context, engine, columns = 3) {
  override fun load() {
    list.setRows(generate(Settings.count, ::Photo).map { PhotoRow(it) }, RowChange.Reset)
  }
}

/*
 * Routes by the names the iOS and React Native examples use.
 */
object Routes {
  val names = listOf("Feed", "Chat", "SectionList", "Masonry")

  // Feature screens, which always run on ShadowListKitListView.
  val features = listOf("Reorder", "ReorderGrid", "Snap", "Horizontal", "Changes", "Collapsing", "Sections", "Inbox")

  fun make(name: String, context: Context, engine: Engine): Screen? = when (name) {
    "Feed" -> FeedScreen(context, engine)
    "Chat" -> ChatScreen(context, engine)
    "SectionList" -> DirectoryScreen(context, engine)
    "Masonry" -> GalleryScreen(context, engine)
    "Reorder" -> ReorderScreen(context, engine, 1)
    "ReorderGrid" -> ReorderScreen(context, engine, 3)
    "Snap" -> SnapScreen(context, engine)
    "Horizontal" -> HorizontalScreen(context, engine)
    "Changes" -> ChangesScreen(context, engine)
    "Collapsing" -> CollapsingScreen(context, engine)
    "Sections" -> SectionsScreen(context)
    "Inbox" -> InboxScreen(context)
    else -> null
  }
}

/*
 * Opens the route from the launch extras, or a list of routes and engines.
 */
class MainActivity : Activity() {
  private var screen: Screen? = null

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    Settings.read(intent)
    LaunchArgs.set(intent)
    val route = Settings.route
    val screen = route?.let { Routes.make(it, this, Settings.engine) }
    if (screen == null) {
      setContentView(home())
      return
    }
    this.screen = screen
    window.decorView.setBackgroundColor(Theme.background)
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
    frame.setBackgroundColor(Theme.background)
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
