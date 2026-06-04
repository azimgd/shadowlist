#pragma once

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
   * Stable user-provided key, used to reconcile elements across updates
   */
  std::string key = "";

  /*
   * Index of the element in the list
   */
  std::size_t index = 0;

  /*
   * Width of an element
   */
  double width = 0.0;
  
  /*
   * Height of an element
   */
  double height = 0.0;
  
  /*
   * Position from left
   */
  double offsetX = 0.0;
  
  /*
   * Position from top
   */
  double offsetY = 0.0;
  
  /*
   * Gap from left
   */
  double gapX = 0.0;
  
  /*
   * Gap from top
   */
  double gapY = 0.0;

  /*
   * Has element been estimated
   */
  bool estimated = false;

  /*
   * Has element been measured
   */
  bool measured = false;
  
  /*
   * Random element id generation
   */
  static std::string generateRandomId() {
    thread_local std::mt19937_64 gen(std::random_device{}());
    thread_local std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << dis(gen);
    return ss.str();
  }
};

}
