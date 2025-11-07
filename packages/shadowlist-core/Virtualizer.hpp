#ifndef Virtualizer_hpp
#define Virtualizer_hpp

#include <cstddef>

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Element.hpp>
#include <shadowlist-core/Error.hpp>

namespace azimgd::shadowlist {

class Virtualizer {
public:
  /*
   * Measure elements
   */
  void measure(Container *container);

  /*
   * Measure elements in default order by index
   */
  void measureDefault(Container *container);

  /*
   * Measure elements in reverse order by index
   */
  void measureInverted(Container *container);

  /*
   * Add element at specific index
   * prevElementIndex: optional index to use for measurement callback (defaults to insertion index)
   */
  static void addElementAtIndex(Container* container, std::size_t index, std::size_t prevElementIndex = (std::size_t)-1);

  /*
   * Update measurements for existing element at specific index
   */
  static void updateElementAtIndex(Container* container, std::size_t index, Size size);

  /*
   * Prepend multiple elements to the beginning of the list
   * Iterates from end of incoming array to maintain correct order
   */
  static void prependElements(Container* container, std::size_t count);

  /*
   * Append multiple elements to the end of the list
   */
  static void appendElements(Container* container, std::size_t count);

private:
  /*
   * Measure elements in default order for the first revision
   */
  void measureFirstRevisionDefault(Container *container);

  /*
   * Measure elements in default order for the subsequent revisions
   */
  void measureNextRevisionDefault(Container *container);

  /*
   * Measure elements in inverted order for the first revision
   */
  void measureFirstRevisionInverted(Container *container);

  /*
   * Measure elements in inverted order for the subsequent revisions
   */
  void measureNextRevisionInverted(Container *container);
};

}
#endif
