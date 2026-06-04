#ifndef SLT_TestFramework_hpp
#define SLT_TestFramework_hpp

#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <type_traits>

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

struct AssertionError {
  std::string message;
};

[[noreturn]] inline void fail(const std::string& message) {
  throw AssertionError{message};
}

// Best-effort stringify for assertion messages.
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

inline int run() {
  int passed = 0;
  int failed = 0;
  double totalMs = 0.0;
  for (const auto& test : registry()) {
    bool ok = true;
    std::string error;

    auto started = std::chrono::steady_clock::now();
    try {
      test.fn();
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
    if (ok) {
      std::cout << "[PASS] " << timing << "  " << test.name << "\n";
      ++passed;
    } else {
      std::cout << "[FAIL] " << timing << "  " << test.name << "\n       " << error << "\n";
      ++failed;
    }
  }

  char total[24];
  std::snprintf(total, sizeof(total), "%.2f", totalMs);
  std::cout << "\n" << passed << " passed, " << failed << " failed  (" << total << " ms total)\n";
  return failed == 0 ? 0 : 1;
}

}  // namespace slt

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

#endif
