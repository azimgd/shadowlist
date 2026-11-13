/*
 * The optional utility module has no list engine or host state. Inputs and results use
 * the caller's scratch memory; each function delegates to the shared utility helper.
 */
#include <emscripten/emscripten.h>

#include <cstring>
#include <string_view>
#include <vector>

#include <shadowlist-core/utils/Avatar.hpp>
#include <shadowlist-core/utils/PollTally.hpp>
#include <shadowlist-core/utils/RelativeTime.hpp>

using namespace azimgd::shadowlist;

#define SHADOWLIST_UTILS_WASM(name) EMSCRIPTEN_KEEPALIVE ShadowListUtilsCore_##name

/*
 * Result slots mirrored by ShadowListUtilsCore.ts.
 */
enum RelativeTimeSlot : int { TIME_UNIT = 0, TIME_COUNT, TIME_LENGTH };
enum PollTallySlot : int { POLL_TOTAL_VOTES = 0, POLL_LEADING_INDEX, POLL_LENGTH };

extern "C" {

int SHADOWLIST_UTILS_WASM(nativeAvatarColorIndex)(const char* name, int length, int paletteSize) {
  return static_cast<int>(avatarColorIndex(std::string_view(name, length), paletteSize > 0 ? paletteSize : 0));
}

int SHADOWLIST_UTILS_WASM(nativeAvatarInitials)(const char* name, int length, char* result) {
  auto initials = avatarInitials(std::string_view(name, length));
  std::memcpy(result, initials.data(), initials.size());
  return static_cast<int>(initials.size());
}

void SHADOWLIST_UTILS_WASM(nativeRelativeTime)(double dateMs, double nowMs, double* result) {
  auto time = relativeTime(dateMs, nowMs);
  result[TIME_UNIT] = static_cast<int>(time.unit);
  result[TIME_COUNT] = time.count;
}

bool SHADOWLIST_UTILS_WASM(nativeRelativeTimeHasDate)(double dateMs) {
  return relativeTimeHasDate(dateMs);
}

void SHADOWLIST_UTILS_WASM(nativePollTally)(const double* votes, int count, double* result) {
  auto tally = pollTally(std::vector<double>(votes, votes + count));
  result[POLL_TOTAL_VOTES] = tally.totalVotes;
  result[POLL_LEADING_INDEX] = tally.leadingIndex == UNDEFINED_INDEX ? -1.0 : static_cast<double>(tally.leadingIndex);
}

int SHADOWLIST_UTILS_WASM(nativePollPercent)(double votes, double totalVotes) {
  return pollPercent(votes, totalVotes);
}

}
