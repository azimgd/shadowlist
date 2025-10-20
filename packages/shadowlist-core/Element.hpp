#ifndef Element_hpp
#define Element_hpp

#include <string>
#include <random>
#include <sstream>
#include <iomanip>

namespace azimgd::shadowlist {

struct Size {
  double width;
  double height;
};

class Element {
public:
  Element() : id(generateRandomId()) {}

  /*
   * Unique identifier for the element
   */
  std::string id = "";

  /*
   * Index of the element in the list
   */
  std::size_t index = 0;

  /*
   * Width of an element
   */
  double width = 0.0f;
  
  /*
   * Height of an element
   */
  double height = 0.0f;
  
  /*
   * Position from left
   */
  double offsetX = 0.0f;
  
  /*
   * Position from top
   */
  double offsetY = 0.0f;
  
  /*
   * Gap from left
   */
  double gapX = 0.0f;
  
  /*
   * Gap from top
   */
  double gapY = 0.0f;

  /*
   * Has element been measured
   */
  bool measured = false;
  
  /*
   * Random element id generation
   */
  static std::string generateRandomId() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << dis(gen);
    return ss.str();
  }
};

}
#endif
