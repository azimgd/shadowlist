class Offsetter {
  private:
  float* offsets;
  int columns;

  public:
  Offsetter(int numColumns, float headerOffset = 0.0f) : columns(numColumns) {
    offsets = new float[columns]();
    for (int i = 0; i < columns; ++i) {
      offsets[i] = headerOffset;
    }
  }

  void add(int column, float px) {
    if (column >= 0 && column < columns) {
      offsets[column] += px;
    }
  }

  float get(int column) const {
    if (column >= 0 && column < columns) {
      return offsets[column];
    }
    return 0;
  }

  float max() const {
    if (columns == 0) return 0;

    float maxOffset = offsets[0];
    for (int i = 1; i < columns; ++i) {
      if (offsets[i] > maxOffset) {
        maxOffset = offsets[i];
      }
    }
    return maxOffset;
  }

  ~Offsetter() {
    delete[] offsets;
  }
};
