#ifndef Error_hpp
#define Error_hpp

#include <stdexcept>

namespace azimgd::shadowlist {

class ContainerError : public std::logic_error {
public:
  using std::logic_error::logic_error;
};

class InvalidOperationError : public ContainerError {
public:
  explicit InvalidOperationError(const std::string& message) :
    ContainerError(message) {}
};

}
#endif
