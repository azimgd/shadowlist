import ShadowListUtils
import UIKit

/*
 * The fixtures as the items of the library's templates. Every screen maps through here.
 */
enum Items {
  private static let hourMs = 3_600_000.0
  private static let minuteMs = 60_000.0

  /*
   * When the app started. Times in the fixtures count back from it.
   */
  private static let nowMs = Date().timeIntervalSince1970 * 1000

  /*
   * How many posts at the start of the feed -SLStates 1 shows as placeholders.
   */
  private static let placeholderPosts = 3

  /*
   * A post. With states, the first three posts are placeholders still loading.
   */
  static func feed(_ post: FeedPost, states: Bool = false) -> ShadowListUtilsFeedItem {
    ShadowListUtilsFeedItem(
      key: post.id,
      author: ShadowListUtilsFeedAuthor(name: post.author.name, handle: post.handle),
      text: post.text,
      images: post.images.map { ShadowListUtilsFeedImage(url: $0) },
      createdAtMs: nowMs - Double(post.hoursAgo) * hourMs,
      placeholder: states && (0..<placeholderPosts).contains(post.index))
  }

  /*
   * A message with the delivery state the screen gives it.
   */
  static func chat(_ message: ChatMessage, status: ShadowListUtilsChatMessageStatus? = nil) -> ShadowListUtilsChatMessage {
    let minutesAgo = message.index.map { max(0, Settings.count - $0) } ?? 0
    return ShadowListUtilsChatMessage(
      key: message.id,
      author: ShadowListUtilsChatAuthor(key: message.author.name, name: message.author.name),
      isOwn: message.isOwn,
      text: message.text,
      images: message.images,
      createdAtMs: nowMs - Double(minutesAgo) * minuteMs,
      status: status)
  }

  static func contact(_ contact: Contact) -> ShadowListUtilsContactItem {
    ShadowListUtilsContactItem(key: contact.id, name: contact.author.name, subtitle: contact.subtitle)
  }

  static func photo(_ photo: Photo) -> ShadowListUtilsMasonryItem {
    ShadowListUtilsMasonryItem(
      key: photo.id, image: ShadowListUtilsMasonryImage(url: photo.url, width: 1, height: photo.aspect, alt: photo.title), title: photo.title)
  }

  /*
   * A numbered tile. Its shape repeats the gallery's.
   */
  static func tile(_ number: Int) -> ShadowListUtilsReorderTileItem {
    ShadowListUtilsReorderTileItem(
      key: "tile-\(number)",
      title: FixtureStrings.imageTitles[number % FixtureStrings.imageTitles.count],
      label: "\(number)",
      aspectRatio: 200 / Photo.designHeights[number % Photo.designHeights.count])
  }

  static func snap(_ index: Int) -> ShadowListUtilsSnapItem {
    ShadowListUtilsSnapItem(
      key: "snap-\(index)",
      title: FixtureStrings.imageTitles[index % FixtureStrings.imageTitles.count],
      imageUrl: Fixtures.imageUrl(index, width: 800),
      color: AppTheme.colors.avatarPalette[index % AppTheme.colors.avatarPalette.count])
  }

  /*
   * A shelf of twelve pictures under a traveller's name.
   */
  static func shelf(_ index: Int) -> ShadowListUtilsNestedItem {
    let name = FixtureStrings.characterNames[index % FixtureStrings.characterNames.count]
    return ShadowListUtilsNestedItem(
      key: "shelf-\(index)", title: "\(name)'s picks",
      cards: (0..<12).map { card in
        let picture = index * 3 + card
        return ShadowListUtilsNestedCardItem(
          key: "shelf-\(index)-card-\(card)", title: FixtureStrings.imageTitles[picture % FixtureStrings.imageTitles.count],
          imageUrl: Fixtures.imageUrl(picture, width: 400))
      })
  }

  /*
   * A message of the inbox, received 37 minutes after the one below it.
   */
  static func mail(_ index: Int) -> ShadowListUtilsInboxMessage {
    ShadowListUtilsInboxMessage(
      key: "message-\(index)",
      sender: FixtureStrings.characterNames[index % FixtureStrings.characterNames.count],
      subject: FixtureStrings.imageTitles[index % FixtureStrings.imageTitles.count],
      preview: FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.count],
      receivedAtMs: nowMs - Double(max(0, index)) * 37 * minuteMs,
      read: index % 3 == 0,
      flagged: false)
  }
}
