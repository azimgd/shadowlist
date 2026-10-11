package com.shadowlist.kit.example

/*
 * The example data, item for item the same as the UIKit and React Native examples' fixtures.
 * Every value is a pure function of the item's generation index.
 */
object Fixtures {
  fun imageUrl(index: Int, width: Int): String {
    val clean = FixtureStrings.images[index % FixtureStrings.images.size].removePrefix("https://")
    return "https://images.weserv.nl/?url=$clean&q=60&w=$width"
  }

  fun avatarColor(name: String): Int {
    var hash = 0L
    for (unit in name) hash = (hash * 31 + unit.code) % 2_147_483_647
    return Theme.avatarPalette[(hash % Theme.avatarPalette.size).toInt()]
  }

  fun initials(name: String): String {
    val words = name.split(" ").filter { it.isNotEmpty() }
    val first = words.firstOrNull()?.firstOrNull() ?: return ""
    if (words.size > 1) return "$first${words.last().first()}".uppercase()
    return first.toString().uppercase()
  }
}

class Author(val name: String) {
  val initials = Fixtures.initials(name)
  val color = Fixtures.avatarColor(name)
}

class FeedPost(index: Int) {
  val id = "post-$index"
  private val name = FixtureStrings.characterNames[index % FixtureStrings.characterNames.size]
  val author = Author(name)
  val handle = "@" + name.lowercase().replace(" ", "")
  val text = FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.size]
  val time = (index % 24).let { if (it == 0) "now" else "${it}h" }
  val images = List(if (index % 10 == 0) 3 + index % 2 else 1) { Fixtures.imageUrl(index + it, 800) }
}

class ChatMessage(index: Int) {
  val id = "msg-$index"
  val isOwn = index % 3 != 0
  val author = Author(if (isOwn) "Me" else FixtureStrings.avatarNames[index % FixtureStrings.avatarNames.size])
  val images: List<String> = when {
    index > 0 && index % 10 == 0 -> List(4) { Fixtures.imageUrl(index + it, 400) }
    index > 0 && index % 5 == 0 -> listOf(Fixtures.imageUrl(index, 800))
    else -> emptyList()
  }
  val text: String? = if (images.isEmpty()) FixtureStrings.sampleTexts[index % FixtureStrings.sampleTexts.size] else null
}

class Contact(index: Int) {
  val id = "contact-$index"
  val author: Author
  val subtitle = "(${100 + index % 900}) ${200 + index % 800}-${1000 + index % 9000}"

  init {
    val names = FixtureStrings.characterNames.map { it.split(" ") }
    val first = names[index % names.size][0]
    val last = names[(index * 7 + index / 15) % names.size][1]
    author = Author("$first $last")
  }
}

class Photo(index: Int) {
  companion object {
    val designHeights = floatArrayOf(180f, 220f, 260f, 200f, 240f, 280f, 190f, 230f, 250f, 210f)
  }

  val id = "photo-$index"
  val title = FixtureStrings.imageTitles[index % FixtureStrings.imageTitles.size]
  val url = Fixtures.imageUrl(index, 400)

  /*
   * Height over width of the image.
   */
  val aspect = Math.round(400 * designHeights[index % designHeights.size] / 122f) / 400f
}
