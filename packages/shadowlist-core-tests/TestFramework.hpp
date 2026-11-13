#pragma once

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

extern char** environ;

namespace slt {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

/*
 * What a helper header adds to the run. variables names the environment variables it reads.
 * check runs before the first test and returns what is wrong with their values, or nothing.
 * report runs after the last test with the path of this binary.
 */
struct RunExtension {
  std::vector<std::string> variables;
  std::function<std::string()> check;
  std::function<void(const std::string& program)> report;
};

inline std::vector<RunExtension>& runExtensions() {
  static std::vector<RunExtension> extensions;
  return extensions;
}

struct RunExtensionRegistrar {
  explicit RunExtensionRegistrar(RunExtension extension) { runExtensions().push_back(std::move(extension)); }
};

struct AssertionError {
  std::string message;
};

[[noreturn]] inline void fail(const std::string& message) {
  throw AssertionError{message};
}

struct SkippedTest {};

/*
 * Leave the running test without a result. The run lists it as skipped.
 */
[[noreturn]] inline void skip() {
  throw SkippedTest{};
}

/*
 * Turn a value into text for a failed check message.
 */
template <typename T>
std::string toStr(const T& value) {
  if constexpr (std::is_same_v<T, std::string>) {
    return value;
  } else if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else if constexpr (std::is_arithmetic_v<T>) {
    return std::to_string(value);
  } else {
    return "<?>";
  }
}

/*
 * Which tests run, read from the environment. Both match any part of a test's name.
 *
 *   SLT_ONLY=<text>  run the tests whose name contains the text
 *   SLT_SKIP=<text>  leave out the tests whose name contains the text
 *
 * A test left out is not run, printed or counted.
 */
struct RunFilter {
  std::string only;
  std::string skip;

  bool admits(const std::string& name) const {
    return (only.empty() || name.find(only) != std::string::npos) &&
      (skip.empty() || name.find(skip) == std::string::npos);
  }
};

inline constexpr const char* RUN_VARIABLE_PREFIX = "SLT_";
inline constexpr const char* RUN_ONLY_VARIABLE = "SLT_ONLY";
inline constexpr const char* RUN_SKIP_VARIABLE = "SLT_SKIP";

/*
 * The exit status of a run that stopped before its first test over its environment.
 */
inline constexpr int RUN_ENVIRONMENT_ERROR = 2;

/*
 * What is wrong with the run's environment, or nothing: a variable with the runner's prefix
 * that nothing reads, one set to nothing, a value an extension rejects, or a filter that names
 * no test or leaves none.
 */
inline std::string checkRunEnvironment(const RunFilter& filter) {
  std::vector<std::string> known = {RUN_ONLY_VARIABLE, RUN_SKIP_VARIABLE};
  for (const RunExtension& extension : runExtensions()) {
    known.insert(known.end(), extension.variables.begin(), extension.variables.end());
  }
  for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
    std::string assignment = *entry;
    if (assignment.rfind(RUN_VARIABLE_PREFIX, 0) != 0) {
      continue;
    }
    std::size_t equals = assignment.find('=');
    std::string name = assignment.substr(0, equals);
    bool read = false;
    for (const std::string& variable : known) {
      read = read || variable == name;
    }
    if (!read) {
      return name + " is not a variable the tests read";
    }
    if (equals == std::string::npos || equals + 1 == assignment.size()) {
      return name + " is set to nothing";
    }
  }
  for (const RunExtension& extension : runExtensions()) {
    std::string wrong = extension.check ? extension.check() : "";
    if (!wrong.empty()) {
      return wrong;
    }
  }

  bool onlyMatches = false;
  bool skipMatches = false;
  bool anyAdmitted = false;
  for (const TestCase& test : registry()) {
    onlyMatches = onlyMatches || test.name.find(filter.only) != std::string::npos;
    skipMatches = skipMatches || test.name.find(filter.skip) != std::string::npos;
    anyAdmitted = anyAdmitted || filter.admits(test.name);
  }
  if (!filter.only.empty() && !onlyMatches) {
    return std::string(RUN_ONLY_VARIABLE) + "=" + filter.only + " matches no test";
  }
  if (!filter.skip.empty() && !skipMatches) {
    return std::string(RUN_SKIP_VARIABLE) + "=" + filter.skip + " matches no test";
  }
  if (!anyAdmitted) {
    return std::string(RUN_ONLY_VARIABLE) + " and " + RUN_SKIP_VARIABLE + " leave no test to run";
  }
  return "";
}

/*
 * Run every registered test the filter admits. program is the path of this binary, for
 * reports that print a command. Returns the exit status: 0 when no test failed, 1 when one
 * did, RUN_ENVIRONMENT_ERROR when the environment was rejected and nothing ran.
 */
inline int run(const std::string& program) {
  RunFilter filter;
  if (const char* value = std::getenv(RUN_ONLY_VARIABLE)) {
    filter.only = value;
  }
  if (const char* value = std::getenv(RUN_SKIP_VARIABLE)) {
    filter.skip = value;
  }
  std::string wrong = checkRunEnvironment(filter);
  if (!wrong.empty()) {
    std::cout << "error: " << wrong << "\n";
    return RUN_ENVIRONMENT_ERROR;
  }

  int passed = 0;
  int failed = 0;
  int skipped = 0;
  int leftOut = 0;
  double totalMs = 0.0;
  for (const auto& test : registry()) {
    if (!filter.admits(test.name)) {
      ++leftOut;
      continue;
    }
    bool ok = true;
    bool ran = true;
    std::string error;

    auto started = std::chrono::steady_clock::now();
    try {
      test.fn();
    } catch (const SkippedTest&) {
      ran = false;
    } catch (const AssertionError& e) {
      ok = false;
      error = e.message;
    } catch (const std::exception& e) {
      ok = false;
      error = std::string("threw: ") + e.what();
    } catch (...) {
      ok = false;
      error = "threw unknown exception";
    }
    double ms = std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - started).count();
    totalMs += ms;

    char timing[24];
    std::snprintf(timing, sizeof(timing), "%9.3f ms", ms);
    if (!ran) {
      std::cout << "[SKIP] " << timing << "  " << test.name << "\n";
      ++skipped;
    } else if (ok) {
      std::cout << "[PASS] " << timing << "  " << test.name << "\n";
      ++passed;
    } else {
      std::cout << "[FAIL] " << timing << "  " << test.name << "\n       " << error << "\n";
      ++failed;
    }
  }

  for (const RunExtension& extension : runExtensions()) {
    if (extension.report) {
      extension.report(program);
    }
  }

  char total[24];
  std::snprintf(total, sizeof(total), "%.2f", totalMs);
  std::cout << "\n" << passed << " passed, " << failed << " failed";
  if (skipped > 0) {
    std::cout << ", " << skipped << " skipped";
  }
  if (leftOut > 0) {
    std::cout << ", " << leftOut << " left out";
  }
  std::cout << "  (" << total << " ms total)\n";
  return failed == 0 ? 0 : 1;
}

}

#define TEST(name)                                                   \
  static void name();                                                \
  static ::slt::Registrar slt_registrar_##name(#name, name);         \
  static void name()

#define CHECK(cond)                                                  \
  do {                                                               \
    if (!(cond)) {                                                   \
      ::slt::fail(std::string("CHECK failed: " #cond) +              \
        "  @ " __FILE__ ":" + std::to_string(__LINE__));             \
    }                                                                \
  } while (0)

#define CHECK_EQ(a, b)                                               \
  do {                                                               \
    auto slt_a = (a);                                                \
    auto slt_b = (b);                                                \
    if (!(slt_a == slt_b)) {                                         \
      ::slt::fail(std::string("CHECK_EQ failed: " #a " == " #b) +    \
        "  [" + ::slt::toStr(slt_a) + " != " + ::slt::toStr(slt_b) + \
        "]  @ " __FILE__ ":" + std::to_string(__LINE__));           \
    }                                                                \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                        \
  do {                                                               \
    double slt_a = (a);                                              \
    double slt_b = (b);                                              \
    if (std::fabs(slt_a - slt_b) > (eps)) {                          \
      ::slt::fail(std::string("CHECK_NEAR failed: " #a " ~= " #b) +  \
        "  [" + ::slt::toStr(slt_a) + " vs " + ::slt::toStr(slt_b) + \
        ", eps " #eps "]  @ " __FILE__ ":" + std::to_string(__LINE__)); \
    }                                                                \
  } while (0)

