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
    label = "${number + 1}",
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
    val categories = listOf("Weekend Escapes", "Island Hopping", "Dark-Sky Destinations", "Northern Lights Trips", "Mountain Airports", "City Breaks", "Desert Stargazing", "Scenic Approaches", "Last-Minute Deals", "Bucket-List Flights")
    val name = categories[index % categories.size]
    return ShadowListUtilsNestedItem("shelf-$index", name, List(10) {
      val picture = index * 10 + it
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
    sender = FixtureStrings.characterNames[(index * 5) % FixtureStrings.characterNames.size],
    subject = mailSubjects[index % mailSubjects.size],
    preview = mailPreviews[(index * 3) % mailPreviews.size],
    receivedAtMs = nowMs - index.coerceAtLeast(0) * 37 * MINUTE_MS,
    read = index % 3 != 0,
    flagged = index % 11 == 4,
  )

  private val mailSubjects = listOf(
    "Gate change for the 6:40 to Reykjavik",
    "Your seat upgrade request",
    "Photos from the aurora night",
    "Itinerary for the coast trip",
    "Re: Packing list",
    "Lounge access this weekend",
    "Boarding pass inside",
    "Dinner on the last night?",
    "Weather looks clear for Thursday",
    "Receipt for your booking",
  )

  private val mailPreviews = listOf(
    "Boarding moved to B14 and starts twenty minutes later than planned. Grab a coffee on the way.",
    "Good news, there is room up front. Confirm before midnight and the upgrade is yours.",
    "I finally sorted through the shots from the ridge. The green band over the lake came out best.",
    "Day one is the drive along the cliffs, day two the lighthouse, day three nothing at all.",
    "Adding a rain shell and the small tripod. Anything else you want me to bring along?",
    "The lounge by gate C opens at five. Show the card at the desk and they will let two in.",
    "Here is your pass for tomorrow. Keep it handy, the scanner at security is slow.",
    "There is a tiny place by the harbour that does grilled fish. Want me to book a table?",
    "Low cloud clears by noon, light wind from the southwest. Perfect for the walk out.",
    "Thanks for booking with us. Your total and the cancellation terms are below.",
  )
}
