namespace facebook::react {

int adjustVisibleStartIndex(
  int visibleStartIndex,
  int childrenMeasurementsTreeSize,
  int initialNumToRender = 10,
  int offset = 5) {
  int visibleEndIndexMin = 0;
  int adjusted = std::max(visibleStartIndex - offset, visibleEndIndexMin);
  return adjusted;
}

int adjustVisibleEndIndex(
  int visibleEndIndex,
  int childrenMeasurementsTreeSize,
  int initialNumToRender = 10,
  int offset = 5) {
  int visibleEndIndexMax = childrenMeasurementsTreeSize - 2;
  int adjusted = std::min(visibleEndIndex + offset, visibleEndIndexMax);
  return adjusted == 0 ? initialNumToRender : adjusted;
}

}
