#pragma once

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

/*
 * Seed loop, campaign report, determinism replay, coverage counters, random picks and the
 * episode log shared by the fuzz test files.
 */
namespace slt {

/*
 * In a default run every seed that divides by this is run twice and the two runs compared.
 * A campaign replays every seed.
 */
inline constexpr std::uint32_t FUZZ_REPLAY_EVERY = 20;

/*
 * Seeds a campaign has to run before a test's rare kinds are required of it.
 */
inline constexpr std::uint32_t FUZZ_RARE_KIND_SEEDS = 5000;

/*
 * The offset basis and the prime of 64 bit FNV-1a, the hash an episode's digest is.
 */
inline constexpr std::uint64_t FUZZ_DIGEST_BASIS = 14695981039346656037ull;
inline constexpr std::uint64_t FUZZ_DIGEST_PRIME = 1099511628211ull;

/*
 * How far a row or an offset may sit from where a check expects it.
 */
inline constexpr double FUZZ_TOLERANCE = 1.0;

/*
 * The frames a simulated host gives a correction to settle.
 */
inline constexpr int FUZZ_MAX_SETTLE_FRAMES = 24;

/*
 * The steps a Trail keeps for a failure message.
 */
inline constexpr std::size_t FUZZ_TRAIL_STEPS = 60;

inline constexpr const char* FUZZ_TRACE_VARIABLE = "SLT_FUZZ_TRACE";
inline constexpr const char* FUZZ_SEEDS_VARIABLE = "SLT_FUZZ_SEEDS";
inline constexpr const char* FUZZ_FIRST_SEED_VARIABLE = "SLT_FUZZ_FIRST_SEED";
inline constexpr const char* FUZZ_DEFAULT_MIX_VARIABLE = "SLT_FUZZ_DEFAULT_MIX";

/*
 * How the fuzz tests run, read once from the environment.
 *
 *   SLT_FUZZ_TRACE=<seed>       print every frame of that seed
 *   SLT_FUZZ_SEEDS=<count>      seeds per test, in place of each test's own count
 *   SLT_FUZZ_FIRST_SEED=<seed>  the first seed, in place of 1
 *   SLT_FUZZ_DEFAULT_MIX=1      keep a campaign to the tests and actions of a default run
 *
 * SLT_FUZZ_SEEDS or SLT_FUZZ_FIRST_SEED turns the run into a campaign. A campaign keeps going
 * past a failing seed and reports every kind of failure once the tests are done. It also runs
 * the campaign mix: the tests registered with forEachCampaignSeed, and the actions and checks
 * a test switches on FuzzEpisode::campaignMix. An excluded test, action or check has a comment
 * that starts with "Campaign mix only" and says why. A default run leaves those out.
 */
struct FuzzOptions {
  std::optional<std::uint32_t> traceSeed;
  std::optional<std::uint32_t> seedCount;
  std::uint32_t firstSeed = 1;
  bool campaign = false;
  bool campaignMix = false;

  /*
   * What is wrong with a variable's value. Empty when every value reads.
   */
  std::string error;
};

/*
 * The whole number a variable is set to. Returns nothing for a variable that is not set, and
 * for a value that is not a number of 32 bits, which it files in error.
 */
inline std::optional<std::uint32_t> readFuzzNumber(const char* variable, std::string& error) {
  const char* value = std::getenv(variable);
  if (value == nullptr) {
    return std::nullopt;
  }
  std::string text = value;
  bool digits = !text.empty() && text.size() <= 10 &&
    std::all_of(text.begin(), text.end(), [](unsigned char letter) { return std::isdigit(letter) != 0; });
  unsigned long long number = digits ? std::strtoull(value, nullptr, 10) : 0;
  if (!digits || number > std::numeric_limits<std::uint32_t>::max()) {
    if (error.empty()) {
      error = std::string(variable) + "=" + text + " is not a whole number of 32 bits";
    }
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(number);
}

inline const FuzzOptions& fuzzOptions() {
  static const FuzzOptions options = [] {
    FuzzOptions read;
    read.traceSeed = readFuzzNumber(FUZZ_TRACE_VARIABLE, read.error);
    read.seedCount = readFuzzNumber(FUZZ_SEEDS_VARIABLE, read.error);
    if (read.seedCount && *read.seedCount == 0 && read.error.empty()) {
      read.error = std::string(FUZZ_SEEDS_VARIABLE) + "=0 runs no seed";
    }
    std::optional<std::uint32_t> firstSeed = readFuzzNumber(FUZZ_FIRST_SEED_VARIABLE, read.error);
    read.firstSeed = firstSeed.value_or(1);
    read.campaign = read.seedCount.has_value() || firstSeed.has_value();

    const char* defaultMix = std::getenv(FUZZ_DEFAULT_MIX_VARIABLE);
    if (defaultMix != nullptr && std::string(defaultMix) != "1" && read.error.empty()) {
      read.error = std::string(FUZZ_DEFAULT_MIX_VARIABLE) + "=" + defaultMix + " is not 1";
    }
    read.campaignMix = read.campaign && defaultMix == nullptr;
    return read;
  }();
  return options;
}

/*
 * Random picks for the generators. Each one is plain arithmetic on the raw output of
 * std::mt19937, which the standard fixes bit for bit. The distributions of <random> are left
 * to each standard library, and a seed drawn through one is another episode on another platform.
 */
inline int pickInt(std::mt19937& rng, int low, int high) {
  if (high < low) {
    fail("pickInt: no number from " + std::to_string(low) + " to " + std::to_string(high));
  }
  std::uint32_t span = static_cast<std::uint32_t>(high - low) + 1;
  return low + static_cast<int>(static_cast<std::uint32_t>(rng()) % span);
}

/*
 * An index or a count from low to high, both included.
 */
inline std::size_t pickSize(std::mt19937& rng, std::size_t low, std::size_t high) {
  if (high < low) {
    fail("pickSize: no number from " + std::to_string(low) + " to " + std::to_string(high));
  }
  return low + static_cast<std::size_t>(static_cast<std::uint32_t>(rng()) % (high - low + 1));
}

/*
 * A roll from 0 to 99.
 */
inline int pickPercent(std::mt19937& rng) {
  return pickInt(rng, 0, 99);
}

/*
 * True percent times in a hundred.
 */
inline bool chance(std::mt19937& rng, int percent) {
  return pickPercent(rng) < percent;
}

/*
 * A share from 0 to 1 in a hundred steps. Both ends come up, which puts a scroll target
 * exactly on an edge now and then.
 */
inline double pickFraction(std::mt19937& rng) {
  return static_cast<double>(pickPercent(rng)) / 99.0;
}

/*
 * A row size along the scroll axis: mostly a line or two of text, sometimes a card, now and
 * then a row taller than half the window.
 */
inline double pickRowSize(std::mt19937& rng) {
  int bucket = pickInt(rng, 0, 9);
  if (bucket < 6) {
    return 40.0 + static_cast<double>(pickInt(rng, 0, 9)) * 12.0;
  }
  if (bucket < 9) {
    return 120.0 + static_cast<double>(pickInt(rng, 0, 9)) * 30.0;
  }
  return 400.0 + static_cast<double>(pickInt(rng, 0, 9)) * 60.0;
}

inline std::uint64_t mixFuzzDigest(std::uint64_t digest, const void* data, std::size_t size) {
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  for (std::size_t index = 0; index < size; ++index) {
    digest = (digest ^ bytes[index]) * FUZZ_DIGEST_PRIME;
  }
  return digest;
}

/*
 * One run of one seed. The test reports every action its generator picks through reach(), and
 * everything a host could see through observe(). Two runs of a seed must end with the same digest.
 */
struct FuzzEpisode {
  bool tracing = false;

  /*
   * Whether the run includes the actions and checks a default run leaves out, see FuzzOptions.
   */
  bool campaignMix = false;
  std::uint64_t digest = FUZZ_DIGEST_BASIS;
  std::map<std::string, std::uint64_t> reached;

  void reach(const std::string& kind) {
    ++reached[kind];
  }

  template <typename T>
  void observe(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
      digest = mixFuzzDigest(digest, value.data(), value.size());
      // The length keeps "ab" + "c" apart from "a" + "bc".
      observe(value.size());
    } else if constexpr (std::is_floating_point_v<T>) {
      double bits = value;
      digest = mixFuzzDigest(digest, &bits, sizeof(bits));
    } else {
      std::uint64_t bits = static_cast<std::uint64_t>(value);
      digest = mixFuzzDigest(digest, &bits, sizeof(bits));
    }
  }
};

/*
 * The steps of one episode: printed when its seed is traced, folded into the digest and kept
 * for the failure message.
 */
struct Trail {
  FuzzEpisode& episode;
  std::uint32_t seed = 0;
  bool tracing = false;
  std::vector<std::string> log;

  Trail(std::uint32_t seed, FuzzEpisode& episode) : episode(episode), seed(seed), tracing(episode.tracing) {}

  void note(const std::string& line) {
    if (tracing) {
      std::printf("      %s\n", line.c_str());
    }
    episode.observe(line);
    log.push_back(line);
    if (log.size() > FUZZ_TRAIL_STEPS) {
      log.erase(log.begin());
    }
  }

  std::string dump() const {
    std::string steps;
    for (const std::string& line : log) {
      steps += "\n    " + line;
    }
    return steps;
  }

  [[noreturn]] void failWith(const std::string& what) const {
    fail("seed " + std::to_string(seed) + ": " + what + dump());
  }
};

/*
 * What the simulated hosts share: the generator of the episode, the rows it made up and the
 * size each one measures at along the scroll axis.
 */
struct FuzzHost : Trail {
  std::mt19937 rng;
  std::unordered_map<std::string, double> trueSizes;
  std::size_t nextKey = 0;

  FuzzHost(std::uint32_t seed, FuzzEpisode& episode) : Trail(seed, episode), rng(seed) {}

  /*
   * The number makeKeys gave a key: 7 for r7.
   */
  static std::size_t numberOf(const std::string& key) {
    return static_cast<std::size_t>(std::strtoull(key.c_str() + 1, nullptr, 10));
  }

  std::vector<std::string> makeKeys(std::size_t count) {
    std::vector<std::string> made = keysFor(count, "r", nextKey);
    nextKey += count;
    for (const std::string& key : made) {
      trueSizes[key] = pickRowSize(rng);
    }
    return made;
  }
};

/*
 * A row on screen and where its leading edge sits from the viewport start.
 */
struct ScreenRow {
  std::string key;
  double screen = 0.0;
};

/*
 * Whether a hold was clamped to an edge. wanted is the offset that would have kept the row
 * where it was. Outside the scroll range the view rests on the edge it hit instead, within slack.
 */
inline bool isHoldClamped(double wanted, double offset, double maxOffset, double slack = FUZZ_TOLERANCE) {
  return (wanted < 0.0 && offset <= slack) || (wanted > maxOffset && std::fabs(offset - maxOffset) <= slack);
}

/*
 * Where a scroll to a row rests: the row at viewPosition of the room the window leaves it,
 * inside the scroll range.
 */
inline double landingOffset(double rowStart, double rowSize, double viewPosition, double windowSize, double maxOffset) {
  double room = std::max(0.0, windowSize - rowSize);
  return std::min(std::max(rowStart - viewPosition * room, 0.0), maxOffset);
}

/*
 * A reader resting on an edge of a list waits for a page forever unless the edge fired for
 * the rows the list has now. edge is "start" or "end", and firedForCount the row count the
 * edge last fired with.
 */
inline void checkEdgeFired(
  const Trail& trail,
  const std::string& edge,
  std::size_t rowCount,
  std::size_t firedForCount) {
  if (rowCount > 0 && firedForCount != rowCount) {
    trail.failWith(edge + " reached never fired at the " + edge + " of " + std::to_string(rowCount) + " rows");
  }
}

/*
 * The kinds a fuzz test passes to FuzzEpisode::reach. Its own seeds must reach every kind in
 * expected. A kind in rare is one the test's own seeds reach too seldom to count on. It is only
 * required of a campaign of FUZZ_RARE_KIND_SEEDS seeds or more. A kind in campaign belongs to
 * an action only the campaign mix takes, and is required of that mix only. A kind in both
 * lists is required of a campaign mix of FUZZ_RARE_KIND_SEEDS seeds or more.
 */
struct FuzzKinds {
  std::vector<std::string> expected;
  std::vector<std::string> rare;
  std::vector<std::string> campaign;
};

/*
 * Every failure of one kind in one test: the first seed that hit it and how many did. A
 * failure of the whole seed range, like a kind no seed reached, has no seed.
 */
struct FuzzBucket {
  std::string key;
  std::string message;
  std::optional<std::uint32_t> firstSeed;
  std::uint64_t count = 0;
};

struct FuzzReport {
  std::string test;
  std::uint64_t seeds = 0;
  std::uint64_t failedSeeds = 0;

  /*
   * The digests of the seeds folded into one, to compare two runs of a campaign by.
   */
  std::uint64_t digest = FUZZ_DIGEST_BASIS;
  std::vector<FuzzBucket> buckets;
  std::map<std::string, std::uint64_t> reached;
};

inline std::vector<FuzzReport>& fuzzReports() {
  static std::vector<FuzzReport> reports;
  return reports;
}

/*
 * The first line of a failure message, without its seed.
 */
inline std::string fuzzHeadline(const std::string& message) {
  std::string line = message.substr(0, message.find('\n'));
  if (line.rfind("seed ", 0) == 0) {
    std::size_t colon = line.find(": ");
    if (colon != std::string::npos) {
      line.erase(0, colon + 2);
    }
  }
  return line;
}

/*
 * The headline with every number replaced by #. Failures that differ only in the seed, a row
 * key, an offset or a count share one key.
 */
inline std::string fuzzBucketKey(const std::string& message) {
  std::string line = fuzzHeadline(message);
  std::string key;
  std::size_t index = 0;
  auto digitAt = [&](std::size_t at) {
    return at < line.size() && std::isdigit(static_cast<unsigned char>(line[at])) != 0;
  };
  while (index < line.size()) {
    bool negative = line[index] == '-' && digitAt(index + 1) &&
      (index == 0 || std::isalnum(static_cast<unsigned char>(line[index - 1])) == 0);
    if (!digitAt(index) && !negative) {
      key.push_back(line[index++]);
      continue;
    }
    if (negative) {
      ++index;
    }
    while (digitAt(index)) {
      ++index;
    }
    if (index < line.size() && line[index] == '.' && digitAt(index + 1)) {
      ++index;
      while (digitAt(index)) {
        ++index;
      }
    }
    key.push_back('#');
  }
  return key;
}

inline void addFuzzFailure(FuzzReport& report, const std::string& message, std::uint32_t seed) {
  std::string key = fuzzBucketKey(message);
  auto found = std::find_if(report.buckets.begin(), report.buckets.end(),
    [&](const FuzzBucket& bucket) { return bucket.key == key; });
  if (found == report.buckets.end()) {
    report.buckets.push_back({key, fuzzHeadline(message), seed, 0});
    found = report.buckets.end() - 1;
  }
  ++found->count;
}

/*
 * A coverage gap fails the whole seed range. Its key keeps the kind's name whole.
 */
inline void addFuzzGap(FuzzReport& report, const std::string& gap) {
  report.buckets.push_back({gap, gap, std::nullopt, 1});
}

/*
 * What one run of a seed ended with: the digest of what it observed, the kinds it reached and
 * the failure message, empty when every check held.
 */
struct FuzzOutcome {
  std::uint64_t digest = 0;
  std::string failure;
  std::map<std::string, std::uint64_t> reached;
};

/*
 * run is called as run(std::uint32_t seed, FuzzEpisode& episode).
 */
template <typename Run>
FuzzOutcome runFuzzEpisode(const Run& run, std::uint32_t seed, bool tracing) {
  FuzzEpisode episode;
  episode.tracing = tracing;
  episode.campaignMix = fuzzOptions().campaignMix;

  FuzzOutcome outcome;
  try {
    run(seed, episode);
  } catch (const AssertionError& e) {
    outcome.failure = e.message;
  } catch (const std::exception& e) {
    outcome.failure = std::string("threw: ") + e.what();
  } catch (...) {
    outcome.failure = "threw unknown exception";
  }
  outcome.digest = episode.digest;
  outcome.reached = std::move(episode.reached);
  return outcome;
}

/*
 * The kinds the seeds were bound to reach and did not, and the kinds they reached that the
 * test does not list or lists for the campaign mix only.
 */
inline std::vector<std::string> fuzzCoverageGaps(
  const FuzzReport& report,
  const FuzzKinds& kinds,
  const std::string& seedRange,
  bool rareRequired) {
  const FuzzOptions& options = fuzzOptions();
  std::vector<std::string> gaps;
  auto requireReached = [&](const std::string& kind) {
    if (report.reached.count(kind) == 0) {
      gaps.push_back("coverage: kind '" + kind + "' was never reached in seeds " + seedRange);
    }
  };
  auto lists = [](const std::vector<std::string>& list, const std::string& kind) {
    return std::find(list.begin(), list.end(), kind) != list.end();
  };
  for (const std::string& kind : kinds.expected) {
    requireReached(kind);
  }
  for (const std::string& kind : kinds.rare) {
    if (rareRequired && (options.campaignMix || !lists(kinds.campaign, kind))) {
      requireReached(kind);
    }
  }
  for (const std::string& kind : kinds.campaign) {
    if (options.campaignMix && !lists(kinds.rare, kind)) {
      requireReached(kind);
    }
  }
  for (const auto& [kind, times] : report.reached) {
    if (lists(kinds.campaign, kind) && !options.campaignMix) {
      gaps.push_back("coverage: kind '" + kind + "' of the campaign mix was reached outside it");
    } else if (!lists(kinds.expected, kind) && !lists(kinds.rare, kind) && !lists(kinds.campaign, kind)) {
      gaps.push_back("coverage: kind '" + kind + "' was reached but is not in the test's list");
    }
  }
  return gaps;
}

/*
 * Run a fuzz test over its seeds, one runFuzzEpisode each. seeds is the test's own count, used
 * unless SLT_FUZZ_SEEDS gives another. kinds lists every kind the run passes to FuzzEpisode::reach.
 *
 * A default run fails at the first failing seed with that seed's message. A campaign runs every
 * seed, files each failure under its bucket for printFuzzCampaign, and fails once all seeds ran.
 *
 * Some seeds run twice, see FUZZ_REPLAY_EVERY. The second run must reach the same digest and the
 * same failure. Across the seeds every expected kind must be reached, and no kind outside the
 * lists. That is only judged when the run covers the test's own seeds, 1 to seeds.
 */
template <typename Run>
void forEachFuzzSeed(const std::string& test, std::uint32_t seeds, const FuzzKinds& kinds, const Run& run) {
  const FuzzOptions& options = fuzzOptions();
  std::uint32_t count = options.seedCount.value_or(seeds);
  std::uint64_t highSeed = static_cast<std::uint64_t>(options.firstSeed) + count - 1;
  if (highSeed > std::numeric_limits<std::uint32_t>::max()) {
    fail(std::to_string(count) + " seeds from " + std::to_string(options.firstSeed) + " pass the last seed of 32 bits");
  }

  fuzzReports().push_back({});
  FuzzReport& report = fuzzReports().back();
  report.test = test;

  for (std::uint32_t index = 0; index < count; ++index) {
    std::uint32_t seed = options.firstSeed + index;
    bool tracing = options.traceSeed && seed == *options.traceSeed;
    FuzzOutcome outcome = runFuzzEpisode(run, seed, tracing);
    ++report.seeds;
    report.digest = mixFuzzDigest(report.digest, &outcome.digest, sizeof(outcome.digest));
    for (const auto& [kind, times] : outcome.reached) {
      report.reached[kind] += times;
    }

    bool failed = !outcome.failure.empty();
    if (failed) {
      if (!options.campaign) {
        fail(outcome.failure);
      }
      addFuzzFailure(report, outcome.failure, seed);
    }

    if (options.campaign || seed % FUZZ_REPLAY_EVERY == 0) {
      FuzzOutcome replay = runFuzzEpisode(run, seed, false);
      bool sameFailure = replay.failure == outcome.failure;
      if (replay.digest != outcome.digest || !sameFailure) {
        std::string message = "seed " + std::to_string(seed) + ": determinism: a replay of the seed " +
          (sameFailure ? "observed different frames" : "ended with a different failure");
        if (!options.campaign) {
          fail(message);
        }
        addFuzzFailure(report, message, seed);
        failed = true;
      }
    }
    if (failed) {
      ++report.failedSeeds;
    }
  }

  bool coversOwnSeeds = options.firstSeed <= 1 && highSeed >= seeds;
  if (coversOwnSeeds) {
    std::string seedRange = std::to_string(options.firstSeed) + ".." + std::to_string(highSeed);
    for (const std::string& gap : fuzzCoverageGaps(report, kinds, seedRange, count >= FUZZ_RARE_KIND_SEEDS)) {
      if (!options.campaign) {
        fail(gap);
      }
      addFuzzGap(report, gap);
    }
  }

  if (!report.buckets.empty()) {
    fail("failing seeds: " + std::to_string(report.failedSeeds) + " of " + std::to_string(report.seeds) +
      ", buckets: " + std::to_string(report.buckets.size()) + ", see the campaign summary");
  }
}

/*
 * forEachFuzzSeed for a test of the campaign mix. A default run skips it.
 */
template <typename Run>
void forEachCampaignSeed(const std::string& test, std::uint32_t seeds, const FuzzKinds& kinds, const Run& run) {
  if (!fuzzOptions().campaignMix) {
    skip();
  }
  forEachFuzzSeed(test, seeds, kinds, run);
}

/*
 * The campaign summary: per test the seeds run, the seeds that failed and the digest of the
 * run, then per bucket its first seed, how many seeds hit it, its message and the command that
 * replays that seed alone with the trace on. program is the path of this binary. Prints nothing
 * outside a campaign.
 */
inline void printFuzzCampaign(const std::string& program) {
  const FuzzOptions& options = fuzzOptions();
  if (!options.campaign || fuzzReports().empty()) {
    return;
  }

  std::string mix = options.campaignMix ? "" : std::string(" ") + FUZZ_DEFAULT_MIX_VARIABLE + "=1";
  std::cout << "\nFuzz campaign, " << (options.campaignMix ? "campaign mix" : "default mix") << "\n";
  for (const FuzzReport& report : fuzzReports()) {
    char digest[24];
    std::snprintf(digest, sizeof(digest), "%016llx", static_cast<unsigned long long>(report.digest));
    std::cout << "\n" << report.test << "\n  seeds run: " << report.seeds << ", from " << options.firstSeed
      << ", failed: " << report.failedSeeds << ", digest: " << digest << "\n";
    for (const FuzzBucket& bucket : report.buckets) {
      if (!bucket.firstSeed) {
        std::cout << "  " << bucket.message << "\n";
        continue;
      }
      std::cout << "  first seed " << *bucket.firstSeed << ", " << bucket.count
        << (bucket.count == 1 ? " seed: " : " seeds: ") << bucket.message << "\n"
        << "    " << RUN_ONLY_VARIABLE << "=" << report.test << " " << FUZZ_FIRST_SEED_VARIABLE << "="
        << *bucket.firstSeed << " " << FUZZ_SEEDS_VARIABLE << "=1" << mix << " " << FUZZ_TRACE_VARIABLE << "="
        << *bucket.firstSeed << " " << program << "\n";
    }
    const char* separator = "  reached: ";
    for (const auto& [kind, times] : report.reached) {
      std::cout << separator << kind << " " << times;
      separator = ", ";
    }
    std::cout << "\n";
  }
}

/*
 * Puts the fuzz variables and the campaign summary into every run of a binary with a fuzz test.
 */
inline const RunExtensionRegistrar FUZZ_RUN_EXTENSION{{
  {FUZZ_TRACE_VARIABLE, FUZZ_SEEDS_VARIABLE, FUZZ_FIRST_SEED_VARIABLE, FUZZ_DEFAULT_MIX_VARIABLE},
  [] { return fuzzOptions().error; },
  printFuzzCampaign,
}};

}
