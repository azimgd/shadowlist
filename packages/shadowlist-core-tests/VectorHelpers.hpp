#pragma once

#include "TestFramework.hpp"

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/utils/Text.hpp>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/*
 * The vector files in vectors/, and a reader for their JSON. A vector file is the contract
 * between a function of the core and its twin in TypeScript: an array of cases, each with a
 * name, an input and the output both sides must give. The jest tests of the package that owns
 * the TypeScript replay the same files, and write the outputs when run with UPDATE_VECTORS=1.
 */
namespace slt {

/*
 * One JSON value: an object, an array, a string, a number, a boolean or null. Strings are UTF-8.
 */
struct Json {
  enum class Kind { Null, Bool, Number, String, Array, Object };

  Kind kind = Kind::Null;
  bool boolean = false;
  double number = 0.0;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;

  /*
   * Set once a replay asked for this field of its object. forEachVector fails a case whose
   * output holds a field its replay never asked for.
   */
  mutable bool read = false;

  bool isNull() const {
    return kind == Kind::Null;
  }

  /*
   * Whether the object has the field and it is not null.
   */
  bool has(const std::string& key) const {
    auto found = fields.find(key);
    if (found == fields.end()) {
      return false;
    }
    found->second.read = true;
    return !found->second.isNull();
  }

  const Json& operator[](const std::string& key) const {
    auto found = fields.find(key);
    if (found == fields.end()) {
      fail("vector has no field '" + key + "'");
    }
    found->second.read = true;
    return found->second;
  }

  const Json& operator[](std::size_t index) const {
    if (index >= items.size()) {
      fail("vector has no item " + std::to_string(index));
    }
    return items[index];
  }

  std::size_t size() const {
    return items.size();
  }

  /*
   * Counts every field below this value as asked for, for a replay that checks a part of an
   * output on purpose.
   */
  void readAll() const {
    for (const auto& field : fields) {
      field.second.read = true;
      field.second.readAll();
    }
    for (const Json& item : items) {
      item.readAll();
    }
  }

  /*
   * Whether a field below this value was never asked for. path then gains that field's place,
   * innermost part first.
   */
  bool findUnreadField(std::vector<std::string>& path) const {
    for (const auto& field : fields) {
      if (!field.second.read || field.second.findUnreadField(path)) {
        path.push_back("." + field.first);
        return true;
      }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
      if (items[index].findUnreadField(path)) {
        path.push_back("[" + std::to_string(index) + "]");
        return true;
      }
    }
    return false;
  }

  /*
   * The path of the first field below this value that no replay asked for, or an empty string.
   */
  std::string firstUnreadField(const std::string& name) const {
    std::vector<std::string> path;
    if (!findUnreadField(path)) {
      return "";
    }
    std::string joined = name;
    for (auto part = path.rbegin(); part != path.rend(); ++part) {
      joined += *part;
    }
    return joined;
  }

  /*
   * A number that stands for an index, where -1 is UNDEFINED_INDEX as JS writes it.
   */
  std::size_t asIndex() const {
    return number < 0.0 ? azimgd::shadowlist::UNDEFINED_INDEX : static_cast<std::size_t>(number);
  }

  /*
   * A number, or NaN for a null, which is how a case leaves an optional number out.
   */
  double asOptionalNumber() const {
    return isNull() ? std::numeric_limits<double>::quiet_NaN() : number;
  }
};

/*
 * An index as a vector file writes it: -1 for UNDEFINED_INDEX.
 */
inline double indexToNumber(std::size_t index) {
  return index == azimgd::shadowlist::UNDEFINED_INDEX ? -1.0 : static_cast<double>(index);
}

struct JsonReader {
  const std::string& source;
  std::size_t cursor = 0;

  char peek() {
    if (cursor >= source.size()) {
      fail("vector file ends inside a value");
    }
    return source[cursor];
  }

  void skipSpace() {
    while (cursor < source.size() && (source[cursor] == ' ' || source[cursor] == '\n' || source[cursor] == '\r' ||
      source[cursor] == '\t')) {
      ++cursor;
    }
  }

  bool take(const char* word) {
    std::string expected = word;
    if (source.compare(cursor, expected.size(), expected) != 0) {
      return false;
    }
    cursor += expected.size();
    return true;
  }

  char32_t readHexUnit() {
    if (cursor + 4 > source.size()) {
      fail("vector file ends inside an escape");
    }
    char32_t unit = static_cast<char32_t>(std::stoul(source.substr(cursor, 4), nullptr, 16));
    cursor += 4;
    return unit;
  }

  std::string readString() {
    ++cursor;
    std::string text;
    while (peek() != '"') {
      char letter = source[cursor++];
      if (letter != '\\') {
        text.push_back(letter);
        continue;
      }
      char escape = peek();
      ++cursor;
      switch (escape) {
        case 'n':
          text.push_back('\n');
          break;
        case 't':
          text.push_back('\t');
          break;
        case 'r':
          text.push_back('\r');
          break;
        case 'b':
          text.push_back('\b');
          break;
        case 'f':
          text.push_back('\f');
          break;
        case 'u': {
          char32_t codePoint = readHexUnit();
          // Two escapes of a surrogate pair are one code point.
          if (codePoint >= 0xD800 && codePoint <= 0xDBFF && take("\\u")) {
            codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (readHexUnit() - 0xDC00);
          }
          azimgd::shadowlist::appendUtf8(text, codePoint);
          break;
        }
        default:
          text.push_back(escape);
      }
    }
    ++cursor;
    return text;
  }

  Json read() {
    skipSpace();
    Json value;
    char letter = peek();
    if (letter == '{') {
      value.kind = Json::Kind::Object;
      ++cursor;
      skipSpace();
      while (peek() != '}') {
        std::string key = readString();
        skipSpace();
        if (peek() != ':') {
          fail("vector file has a field without a value");
        }
        ++cursor;
        value.fields[key] = read();
        skipSpace();
        if (peek() == ',') {
          ++cursor;
        }
        skipSpace();
      }
      ++cursor;
    } else if (letter == '[') {
      value.kind = Json::Kind::Array;
      ++cursor;
      skipSpace();
      while (peek() != ']') {
        value.items.push_back(read());
        skipSpace();
        if (peek() == ',') {
          ++cursor;
        }
        skipSpace();
      }
      ++cursor;
    } else if (letter == '"') {
      value.kind = Json::Kind::String;
      value.text = readString();
    } else if (take("null")) {
      value.kind = Json::Kind::Null;
    } else if (take("true")) {
      value.kind = Json::Kind::Bool;
      value.boolean = true;
    } else if (take("false")) {
      value.kind = Json::Kind::Bool;
    } else {
      value.kind = Json::Kind::Number;
      char* end = nullptr;
      value.number = std::strtod(source.c_str() + cursor, &end);
      std::size_t next = static_cast<std::size_t>(end - source.c_str());
      if (next == cursor) {
        fail("vector file has a value that is not JSON at byte " + std::to_string(cursor));
      }
      cursor = next;
    }
    return value;
  }
};

/*
 * The cases of vectors/<file>. The build passes the folder as SHADOWLIST_VECTORS_DIR.
 */
inline Json loadVectors(const std::string& file) {
  std::string path = std::string(SHADOWLIST_VECTORS_DIR) + "/" + file;
  std::ifstream in(path, std::ios::binary);
  std::stringstream buffer;
  buffer << in.rdbuf();
  std::string text = buffer.str();
  if (text.empty()) {
    fail("cannot read " + path);
  }
  JsonReader reader{text};
  Json cases = reader.read();
  if (cases.kind != Json::Kind::Array || cases.items.empty()) {
    fail(path + " holds no cases");
  }
  return cases;
}

/*
 * Runs replay over every case of a vector file, as replay(const Json& vector). A failed check
 * names the file and the case. A case fails as well when its output holds a field the replay
 * did not ask for, which is a field TypeScript writes and the C++ does not check. Returns how
 * many cases ran.
 */
template <typename Replay>
std::size_t forEachVector(const std::string& file, const Replay& replay) {
  Json cases = loadVectors(file);
  for (const Json& vector : cases.items) {
    try {
      replay(vector);
      std::string unread = vector["output"].firstUnreadField("output");
      if (!unread.empty()) {
        fail("no check reads " + unread);
      }
    } catch (const AssertionError& error) {
      fail(file + ", case '" + vector["name"].text + "': " + error.message);
    }
  }
  return cases.items.size();
}

/*
 * The files VECTOR_TEST replays, gathered before any test runs.
 */
inline std::set<std::string>& replayedVectorFiles() {
  static std::set<std::string> files;
  return files;
}

struct VectorFile {
  explicit VectorFile(const char* file) {
    replayedVectorFiles().insert(file);
  }
};

/*
 * The .json files in vectors/, by name.
 */
inline std::set<std::string> vectorFilesInFolder() {
  std::set<std::string> files;
  for (const auto& entry : std::filesystem::directory_iterator(SHADOWLIST_VECTORS_DIR)) {
    if (entry.path().extension() == ".json") {
      files.insert(entry.path().filename().string());
    }
  }
  return files;
}

/*
 * The cases of a file that holds more than one function, for the function named name.
 */
inline bool isVectorFor(const Json& vector, const std::string& name) {
  return vector["function"].text == name;
}

}

/*
 * A test that replays one vector file. Its body takes each case as const Json& vector. The
 * file counts as replayed, see every_vector_file_has_a_replay.
 */
#define VECTOR_TEST(name, file)                                      \
  static void slt_replay_##name(const ::slt::Json& vector);          \
  static const ::slt::VectorFile slt_vector_file_##name{file};       \
  TEST(name) {                                                       \
    ::slt::forEachVector(file, slt_replay_##name);                   \
  }                                                                  \
  static void slt_replay_##name(const ::slt::Json& vector)
