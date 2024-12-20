#include <string>
#include <optional>
#include <vector>

namespace facebook::react {

class SLKeyExtractor {
  public:
    static std::string extractKey(const std::string& input);
};

}
