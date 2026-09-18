#pragma once

#include <stdexcept>

namespace azimgd::shadowlist {

class InvalidOperationError : public std::logic_error {
public:
  using std::logic_error::logic_error;
};

}
