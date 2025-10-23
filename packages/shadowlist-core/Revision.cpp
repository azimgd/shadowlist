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

std::string Revision::getDebugRepresentation(const RevisionDebugRepresentationMetadata& metadata) const {
  std::ostringstream json;
  json << std::fixed << std::setprecision(2);

  json << "{";
  json << "\"timestamp\":" << timestamp.count() << ",";
  json << "\"timestampDiff\":" << metadata.timestampDiff << ",";
  json << "\"containerOffsetX\":" << containerOffsetX << ",";
  json << "\"containerOffsetY\":" << containerOffsetY << ",";
  json << "\"measurementElementStartIndex\":" << measurementElementStartIndex << ",";
  json << "\"measurementElementEndIndex\":" << measurementElementEndIndex << ",";
  json << "\"measurementElementCount\":" << measurementElementCount << ",";
  json << "\"averageElementWidth\":" << averageElementWidth << ",";
  json << "\"averageElementHeight\":" << averageElementHeight << ",";
  json << "\"measurementElementTotalHeight\":" << measurementElementTotalHeight << ",";
  json << "\"measurementElementTotalWidth\":" << measurementElementTotalWidth << ",";
  json << "\"windowContainerHeight\":" << windowContainerHeight << ",";
  json << "\"windowContainerWidth\":" << windowContainerWidth << ",";
  json << "\"totalContainerHeight\":" << totalContainerHeight << ",";
  json << "\"totalContainerWidth\":" << totalContainerWidth << ",";
  json << "\"mvcpDiffHeight\":" << mvcpDiffHeight << ",";
  json << "\"mvcpDiffWidth\":" << mvcpDiffWidth << ",";

  json << "\"elements\":[";
  for (size_t nextElementIndex = 0; nextElementIndex < elements.size(); ++nextElementIndex) {
    const auto& elem = elements[nextElementIndex];
    json << "{";
    json << "\"id\":\"" << elem.id << "\",";
    json << "\"width\":" << elem.width << ",";
    json << "\"height\":" << elem.height << ",";
    json << "\"offsetX\":" << elem.offsetX << ",";
    json << "\"offsetY\":" << elem.offsetY << ",";
    json << "\"measured\":" << (elem.measured ? "true" : "false");
    json << "}";
    if (nextElementIndex < elements.size() - 1) {
      json << ",";
    }
  }
  json << "]";

  json << "}";

  return json.str();
}

}
