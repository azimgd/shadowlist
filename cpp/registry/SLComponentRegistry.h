#ifndef SLComponentRegistry_h
#define SLComponentRegistry_h

#include <vector>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <numeric>
#include <algorithm>

class SLComponent {
public:
  SLComponent(std::string uniqueId, int index) : index(index), uniqueId(uniqueId), isVisible(false) {}
  int getIndex() const;
  std::string getUniqueId() const;
  bool getVisible() const;
  void setVisible(bool visible);

  bool operator==(const SLComponent& other) const {
    return uniqueId == other.uniqueId && index == other.index;
  }

private:
  int index;
  std::string uniqueId;
  bool isVisible;
};

class SLComponentRegistry {
public:
  using SLObserver = std::function<void(std::string uniqueId, int index, bool isVisible)>;

  SLComponentRegistry();
  void registerComponent(std::string uniqueId, int index);
  void unregisterComponent(std::string uniqueId, int index);
  void mountRange(int visibleStartIndex, int visibleEndIndex);
  void mount(const std::vector<int> &indices);
  void unmount(const std::vector<int> &indices);
  void mountObserver(const SLObserver &observer);
  void unmountObserver(const SLObserver &observer);

private:
  std::unordered_map<int, SLComponent> components;
  std::vector<SLObserver> observers;
  int initialNumToRender;

  void updateVisibility(const std::vector<int> &indices, bool visible);
  void notifyObservers(const std::unordered_set<SLComponent> &updatedComponents, bool isVisible);
};

namespace std {
  template <>
  struct hash<SLComponent> {
    size_t operator()(const SLComponent& component) const {
      return hash<std::string>()(component.getUniqueId()) ^ hash<int>()(component.getIndex());
    }
  };
}

#endif
