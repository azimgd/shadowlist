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
   * Measure items in default order by index
   */
  void measureDefault(Container *container);

  /*
   * Measure items in reverse order by index
   */
  void measureInverted(Container *container);

  /*
   * Add item at specific index
   * prevElementIndex: optional index to use for measurement callback (defaults to insertion index)
   */
  static void addElementAtIndex(Container* container, std::size_t index, Revision& nextRevision, std::size_t prevElementIndex = (std::size_t)-1);

  /*
   * Update measurements for existing element at specific index
   * Adjusts mvcpDiff to maintain visible content position when element changes
   */
  static void updateElementAtIndex(Container* container, std::size_t index, Revision& nextRevision, Size size);

  /*
   * Prepend multiple elements to the beginning of the list
   * Iterates from end of incoming array to maintain correct order
   */
  static void prependElements(Container* container, std::size_t count, Revision& nextRevision);

  /*
   * Append multiple elements to the end of the list
   */
  static void appendElements(Container* container, std::size_t count, Revision& nextRevision);

private:
  /*
   * Measure items in default order for the first revision
   */
  void measureFirstRevisionDefault(Container *container, Revision &nextRevision);

  /*
   * Measure items in default order for the subsequent revisions
   */
  void measureNextRevisionDefault(Container *container, Revision &nextRevision);

  /*
   * Measure items in inverted order for the first revision
   */
  void measureFirstRevisionInverted(Container *container, Revision &nextRevision);

  /*
   * Measure items in inverted order for the subsequent revisions
   */
  void measureNextRevisionInverted(Container *container, Revision &nextRevision);
};

}
#endif
