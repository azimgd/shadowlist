#include "SLKeyExtractor.h"

namespace azimgd::shadowlist {

std::string SLKeyExtractor::extractKey(const std::string& input) {
  std::string key = input;
  size_t pos = 0;
  
  while ((pos = key.find("{{", pos)) != std::string::npos) {
    key.erase(pos, 2);
  }

  pos = 0;
  while ((pos = key.find("}}", pos)) != std::string::npos) {
    key.erase(pos, 2);
  }

  return key;
}

}
