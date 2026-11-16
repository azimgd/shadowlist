package com.shadowlist.kit.example

import android.content.Context

/*
 * An example by the route name every platform's example app uses.
 */
class Example(val route: String, val title: String, val summary: String, val make: (Context, Engine) -> Screen)

/*
 * The same titles, summaries and section order as the React Native example.
 */
class ExampleSection(val title: String, val examples: List<Example>) {
  companion object {
    fun example(route: String): Example? = all.firstNotNullOfOrNull { section -> section.examples.firstOrNull { it.route == route } }

    val all = listOf(
      ExampleSection("Feeds", listOf(
        Example("Feed", "Feed", "Posts with photo carousels and pull to refresh") { context, engine -> FeedScreen(context, engine) },
        Example("Masonry", "Gallery", "Photos in three columns") { context, engine -> GalleryScreen(context, engine) },
        Example("Nested", "Explore", "Horizontal shelves inside a vertical list") { context, _ -> NestedScreen(context) },
      )),
      ExampleSection("Messaging", listOf(
        Example("Chat", "Chat", "Group chat that keeps your place as messages arrive") { context, engine -> ChatScreen(context, engine) },
        Example("Assistant", "Assistant", "Streaming replies that grow in place") { context, _ -> AssistantScreen(context) },
        Example("Inbox", "Inbox", "Swipe actions, a menu on long press and selection") { context, _ -> InboxScreen(context) },
      )),
      ExampleSection("Lists", listOf(
        Example("Activity", "Activity", "Opens partway down, loads in both directions") { context, _ -> ActivityScreen(context) },
        Example("Contacts", "Companions", "Swipe a row to remove it") { context, _ -> ContactsScreen(context) },
        Example("SectionList", "Directory", "Sections with sticky headers and an index") { context, engine -> DirectoryScreen(context, engine) },
        Example("Reorder", "Boarding Order", "Touch and hold a row, then drag it") { context, _ -> ReorderScreen(context) },
        Example("ReorderGrid", "Sky Wishlist", "Touch and hold a card, then drag it across the grid") { context, _ -> ReorderGridScreen(context) },
        Example("Board", "Release Board", "Drag cards within a column or across columns") { context, _ -> BoardScreen(context) },
        Example("Tree", "Trip Files", "Folders that expand and collapse") { context, _ -> TreeScreen(context) },
        Example("Snap", "Destinations", "Full-screen cards that snap into place") { context, _ -> SnapScreen(context) },
        Example("ScrollLab", "Scroll Lab", "Scroll commands, scroll events and viewability") { context, _ -> ScrollLabScreen(context) },
      )),
      ExampleSection("Components", listOf(
        Example("Templates", "Templates", "Every reusable template on one page") { context, _ -> TemplatesScreen(context) },
      )),
      ExampleSection("Native demos", listOf(
        Example("Poll", "Poll", "Choose a destination and see the votes") { context, _ -> PollScreen(context) },
        Example("Horizontal", "Horizontal", "A horizontal list") { context, engine -> HorizontalScreen(context, engine) },
        Example("Changes", "Changes", "Insert, remove and update rows") { context, engine -> ChangesScreen(context, engine) },
        Example("Collapsing", "Collapsing", "A list with a collapsing header") { context, engine -> CollapsingScreen(context, engine) },
        Example("Sections", "Sections", "Section headers, footers and an index") { context, _ -> SectionsScreen(context) },
      )),
    )
  }
}
