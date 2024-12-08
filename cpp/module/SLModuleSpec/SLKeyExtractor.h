#include <string>
#include <optional>
#include <vector>

namespace facebook::react {

class SLKeyExtractor {
  public:
    static std::optional<std::string> extractKey(const std::string& input);
    static std::vector<std::string> extractAllKeys(const std::string& input);
};

}
