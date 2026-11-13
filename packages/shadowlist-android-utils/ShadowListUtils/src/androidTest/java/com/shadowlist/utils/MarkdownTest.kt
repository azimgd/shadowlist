package com.shadowlist.utils

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test

class MarkdownTest {
  @Test fun the_step_bridge_replays_every_shared_integer_utf16_fixture() {
    for (entry in Vectors.cases("assistant-steps.json")) {
      val input = entry.getJSONObject("input")
      val positions = input.getJSONArray("positions")
      val steps = ShadowListUtilsCore.stepsOf(input.getString("content"), LongArray(positions.length()) { positions.optLong(it, 0) })
      val expected = entry.getJSONArray("output")
      assertEquals(expected.length(), steps.size)
      steps.forEachIndexed { index, step ->
        val value = expected.getJSONObject(index)
        when (step) {
          is ShadowListUtilsAssistantStep.Text -> {
            assertEquals("text", value.getString("kind"))
            assertEquals(value.getString("text"), step.text)
            assertEquals(value.getInt("at"), step.at)
          }
          is ShadowListUtilsAssistantStep.Calls -> {
            assertEquals("calls", value.getString("kind"))
            val indices = value.getJSONArray("callIndices")
            assertEquals((0 until indices.length()).map { indices.getInt(it) }, step.callIndices.toList())
          }
        }
      }
    }
  }

  @Test fun the_typed_bridge_replays_shared_markdown_and_url_vectors() {
    fun runs(inlines: Array<ShadowListUtilsMarkdownInline>): JSONArray = JSONArray().apply {
      for (run in inlines) put(JSONObject().apply {
        put("style", run.style.name.lowercase(java.util.Locale.ROOT))
        put("text", run.text)
        if (run.href != null) put("href", run.href)
      })
    }
    for (entry in Vectors.cases("markdown.json")) {
      val input = entry.getJSONObject("input")
      if (entry.getString("function") == "isSafeUrl") {
        assertEquals(entry.getString("name"), entry.getBoolean("output"), ShadowListUtilsCore.isSafeUrl(input.getString("url")))
        continue
      }
      val chunks = input.getJSONArray("chunks")
      val source = (0 until chunks.length()).joinToString("") { chunks.getString(it) }
      val actual = JSONArray()
      for (block in ShadowListUtilsCore.parseMarkdown(source)) actual.put(JSONObject().apply {
        put("key", block.key)
        put("type", block.type.name.lowercase(java.util.Locale.ROOT))
        put("raw", block.raw)
        when (block.type) {
          ShadowListUtilsMarkdownBlock.Type.HEADING -> { put("level", block.level); put("inlines", runs(block.runs.firstOrNull() ?: emptyArray())) }
          ShadowListUtilsMarkdownBlock.Type.PARAGRAPH, ShadowListUtilsMarkdownBlock.Type.QUOTE -> put("inlines", runs(block.runs.firstOrNull() ?: emptyArray()))
          ShadowListUtilsMarkdownBlock.Type.LIST -> { put("ordered", block.ordered); put("start", block.start); put("items", JSONArray().apply { block.runs.forEach { put(runs(it)) } }) }
          ShadowListUtilsMarkdownBlock.Type.CODE -> { put("language", block.language); put("code", block.code); put("closed", block.closed) }
          ShadowListUtilsMarkdownBlock.Type.TABLE -> {
            var at = 0
            val rows = ArrayList<JSONArray>()
            for (count in block.rowCellCounts) {
              rows.add(JSONArray().apply { repeat(count) { put(runs(block.runs[at++])); } })
            }
            put("header", rows.firstOrNull() ?: JSONArray())
            put("rows", JSONArray(rows.drop(1)))
          }
          ShadowListUtilsMarkdownBlock.Type.RULE -> Unit
        }
      })
      assertEquals(entry.getString("name"), canonical(entry.getJSONObject("output").getJSONArray("blocks")), canonical(actual))
    }
  }

  private fun canonical(value: Any): String = when (value) {
    is JSONObject -> value.keys().asSequence().sorted().joinToString(prefix = "{", postfix = "}") { "$it:${canonical(value.get(it))}" }
    is JSONArray -> (0 until value.length()).joinToString(prefix = "[", postfix = "]") { canonical(value.get(it)) }
    is Number -> value.toDouble().toString()
    else -> value.toString()
  }
}
