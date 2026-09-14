#include <shadowlist-core/Revision.hpp>

#include <iomanip>
#include <sstream>

namespace azimgd::shadowlist {

void Revision::setWindowContainerHeight(double windowContainerHeight) {
  this->windowContainerHeight = windowContainerHeight;
}

void Revision::setWindowContainerWidth(double windowContainerWidth) {
  this->windowContainerWidth = windowContainerWidth;
}

void Revision::setContainerOffsetX(double containerOffsetX) {
  this->containerOffsetX = containerOffsetX;
}

void Revision::setContainerOffsetY(double containerOffsetY) {
  this->containerOffsetY = containerOffsetY;
}

std::string Revision::getDebugRepresentation() const {
  std::ostringstream json;
  json << std::fixed << std::setprecision(2);

  json << "{";
  json << "\"containerOffsetX\":" << this->containerOffsetX << ",";
  json << "\"containerOffsetY\":" << this->containerOffsetY << ",";
  json << "\"measurementElementStartIndex\":" << this->measurementElementStartIndex << ",";
  json << "\"measurementElementEndIndex\":" << this->measurementElementEndIndex << ",";
  json << "\"measurementElementCount\":" << this->measurementElementCount << ",";
  json << "\"averageElementWidth\":" << this->averageElementWidth << ",";
  json << "\"averageElementHeight\":" << this->averageElementHeight << ",";
  json << "\"measurementElementTotalHeight\":" << this->measurementElementTotalHeight << ",";
  json << "\"measurementElementTotalWidth\":" << this->measurementElementTotalWidth << ",";
  json << "\"windowContainerHeight\":" << this->windowContainerHeight << ",";
  json << "\"windowContainerWidth\":" << this->windowContainerWidth << ",";
  json << "\"totalContainerHeight\":" << this->totalContainerHeight << ",";
  json << "\"totalContainerWidth\":" << this->totalContainerWidth << ",";

  json << "\"elements\":[";
  for (std::size_t nextElementIndex = 0; nextElementIndex < this->elements.size(); ++nextElementIndex) {
    const Element& nextElement = this->elements[nextElementIndex];
    json << "{";
    json << "\"id\":\"" << nextElement.getId() << "\",";
    json << "\"width\":" << nextElement.width << ",";
    json << "\"height\":" << nextElement.height << ",";
    json << "\"offsetX\":" << nextElement.offsetX << ",";
    json << "\"offsetY\":" << nextElement.offsetY << ",";
    json << "\"estimated\":" << (nextElement.estimated ? "true" : "false") << ",";
    json << "\"measured\":" << (nextElement.measured ? "true" : "false");
    json << "}";
    if (nextElementIndex < this->elements.size() - 1) {
      json << ",";
    }
  }
  json << "]";

  json << "}";

  return json.str();
}

}
