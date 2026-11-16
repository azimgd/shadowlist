import UIKit

/*
 * An example by the route name every platform's example app uses.
 */
struct Example {
  let route: String
  let title: String
  let summary: String
  let make: () -> UIViewController
}

/*
 * The same titles, summaries and section order as the React Native example.
 */
struct ExampleSection {
  let title: String
  let examples: [Example]

  static func example(route: String) -> Example? {
    all.lazy.flatMap(\.examples).first { $0.route == route }
  }

  static let all: [ExampleSection] = [
    ExampleSection(title: "Feeds", examples: [
      Example(route: "Feed", title: "Feed", summary: "Posts with photo carousels and pull to refresh") { FeedScreen() },
      Example(route: "Masonry", title: "Gallery", summary: "Photos in three columns") { GalleryScreen() },
      Example(route: "Nested", title: "Explore", summary: "Horizontal shelves inside a vertical list") { NestedScreen() },
    ]),
    ExampleSection(title: "Messaging", examples: [
      Example(route: "Chat", title: "Chat", summary: "Group chat that keeps your place as messages arrive") { ChatScreen() },
      Example(route: "Assistant", title: "Assistant", summary: "Streaming replies that grow in place") { AssistantScreen() },
      Example(route: "Inbox", title: "Inbox", summary: "Swipe actions, a menu on long press and selection") { InboxScreen() },
    ]),
    ExampleSection(title: "Lists", examples: [
      Example(route: "Activity", title: "Activity", summary: "Opens partway down, loads in both directions") { ActivityScreen() },
      Example(route: "Contacts", title: "Companions", summary: "Swipe a row to remove it") { ContactsScreen() },
      Example(route: "SectionList", title: "Directory", summary: "Sections with sticky headers and an index") { DirectoryScreen() },
      Example(route: "Reorder", title: "Boarding Order", summary: "Touch and hold a row, then drag it") { ReorderScreen() },
      Example(route: "ReorderGrid", title: "Sky Wishlist", summary: "Touch and hold a card, then drag it across the grid") { ReorderGridScreen() },
      Example(route: "Board", title: "Release Board", summary: "Drag cards within a column or across columns") { BoardScreen() },
      Example(route: "Tree", title: "Trip Files", summary: "Folders that expand and collapse") { TreeScreen() },
      Example(route: "Snap", title: "Destinations", summary: "Full-screen cards that snap into place") { SnapScreen() },
      Example(route: "ScrollLab", title: "Scroll Lab", summary: "Scroll commands, scroll events and viewability") { ScrollLabScreen() },
    ]),
    ExampleSection(title: "Components", examples: [
      Example(route: "Templates", title: "Templates", summary: "Every reusable template on one page") { TemplatesScreen() },
    ]),
    ExampleSection(title: "Native demos", examples: [
      Example(route: "Poll", title: "Poll", summary: "Choose a destination and see the votes") { PollScreen() },
      Example(route: "Horizontal", title: "Horizontal", summary: "A horizontal list") { HorizontalScreen() },
      Example(route: "Changes", title: "Changes", summary: "Insert, remove and update rows") { ChangesScreen() },
      Example(route: "Collapsing", title: "Collapsing", summary: "A list with a collapsing header") { CollapsingScreen() },
      Example(route: "Sections", title: "Sections", summary: "Section headers, footers and an index") { SectionsScreen() },
    ]),
  ]
}
