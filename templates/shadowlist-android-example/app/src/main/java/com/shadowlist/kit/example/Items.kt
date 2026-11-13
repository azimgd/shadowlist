package com.shadowlist.kit.example

import com.shadowlist.utils.ShadowListUtilsChatAuthor
import com.shadowlist.utils.ShadowListUtilsChatMessage
import com.shadowlist.utils.ShadowListUtilsChatMessageStatus
import com.shadowlist.utils.ShadowListUtilsContactItem
import com.shadowlist.utils.ShadowListUtilsFeedAuthor
import com.shadowlist.utils.ShadowListUtilsFeedImage
import com.shadowlist.utils.ShadowListUtilsFeedItem
import com.shadowlist.utils.ShadowListUtilsInboxMessage
import com.shadowlist.utils.ShadowListUtilsMasonryImage
import com.shadowlist.utils.ShadowListUtilsMasonryItem
import com.shadowlist.utils.ShadowListUtilsNestedCardItem
import com.shadowlist.utils.ShadowListUtilsNestedItem
import com.shadowlist.utils.ShadowListUtilsReorderTileItem
import com.shadowlist.utils.ShadowListUtilsSnapItem
import com.shadowlist.utils.ShadowListUtilsTheme

private const val HOUR_MS = 3_600_000.0
private const val MINUTE_MS = 60_000.0
private const val PLACEHOLDER_POSTS = 3

/*
 * The fixtures as the items of the library's templates. Every screen maps through here.
 */
object Items {
  /*
   * When the app started. Times in the fixtures count back from it.
   */
  private val nowMs = System.currentTimeMillis().toDouble()

  /*
   * The avatar colors, which both themes share.
   */
  private val palette = ShadowListUtilsTheme.LIGHT.colors.avatarPalette

  /*
   * A post. With states, the first three posts are placeholders still loading.
   */
  fun feed(post: FeedPost, states: Boolean = false) = ShadowListUtilsFeedItem(
    key = post.id,
    author = ShadowListUtilsFeedAuthor(post.author.name, post.handle),
    text = post.text,
    images = post.images.map { ShadowListUtilsFeedImage(it) },
    createdAtMs = nowMs - post.hoursAgo * HOUR_MS,
    placeholder = states && post.index in 0 until PLACEHOLDER_POSTS,
  )

  /*
   * A message with the delivery state the screen gives it.
   */
  fun chat(message: ChatMessage, status: ShadowListUtilsChatMessageStatus? = null) = ShadowListUtilsChatMessage(
    key = message.id,
    author = ShadowListUtilsChatAuthor(message.author.name, message.author.name),
    isOwn = message.isOwn,
    text = message.text,
    images = message.images,
    createdAtMs = nowMs - (Settings.count - message.index).coerceAtLeast(0) * MINUTE_MS,
    status = status,
  )

  fun contact(contact: Contact) = ShadowListUtilsContactItem(contact.id, contact.author.name, contact.subtitle)

  fun photo(photo: Photo) = ShadowListUtilsMasonryItem(
    photo.id, ShadowListUtilsMasonryImage(photo.url, 1f, photo.aspect, photo.title), photo.title)

  /*
   * A numbered tile. Its shape repeats the gallery's.
   */
  fun tile(number: Int) = ShadowListUtilsReorderTileItem(
    key = "tile-$number",
    title = FixtureStrings.imageTitles[number % FixtureStrings.imageTitles.size],
    label = "$number",
    aspectRatio = 200f / Photo.designHeights[number % Photo.designHeights.size],
  )

  fun snap(index: Int) = ShadowListUtilsSnapItem(
    key = "snap-$index",
    title = FixtureStrings.imageTitles[index % FixtureStrings.imageTitles.size],
    imageUrl = Fixtures.imageUrl(index, 800),
    color = palette[index % palette.size],
  )

  /*
   * A shelf of twelve pictures under a traveller's name.
   */
  fun shelf(index: Int): ShadowListUtilsNestedItem {
    val name = FixtureStrings.characterNames[index % FixtureStrings.characterNames.size]
    return ShadowListUtilsNestedItem("shelf-$index", "$name's picks", List(12) {
      val picture = index * 3 + it
      ShadowListUtilsNestedCardItem(
        "shelf-$index-card-$it", FixtureStrings.imageTitles[picture % FixtureStrings.imageTitles.size],
        Fixtures.imageUrl(picture, 400))
    })
  }

  /*
   * A message of the inbox, received 37 minutes after the one below it.
   */
  fun mail(index: Int) = ShadowListUtilsInboxMessage(
    key = "message-$index",
    sender = FixtureStrings.characterNames[index % FixtureStrings.characterNames.size],
    subject = FixtureStrings.imageTitles[index % FixtureStrings.imageTitles.size],
    preview = FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.size],
    receivedAtMs = nowMs - index.coerceAtLeast(0) * 37 * MINUTE_MS,
    read = index % 3 == 0,
    flagged = false,
  )
}
