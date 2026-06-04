#include <shadowlist-core/Revision.hpp>
#include <sstream>
#include <iomanip>

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
    const auto& elem = this->elements[nextElementIndex];
    json << "{";
    json << "\"id\":\"" << elem.id << "\",";
    json << "\"width\":" << elem.width << ",";
    json << "\"height\":" << elem.height << ",";
    json << "\"offsetX\":" << elem.offsetX << ",";
    json << "\"offsetY\":" << elem.offsetY << ",";
    json << "\"estimated\":" << (elem.estimated ? "true" : "false") << ",";
    json << "\"measured\":" << (elem.measured ? "true" : "false");
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
