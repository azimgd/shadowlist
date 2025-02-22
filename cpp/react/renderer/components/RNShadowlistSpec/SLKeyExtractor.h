#include <string>
#include <optional>
#include <vector>

namespace azimgd::shadowlist {

class SLKeyExtractor {
  public:
    static std::string extractKey(const std::string& input);
};

}
