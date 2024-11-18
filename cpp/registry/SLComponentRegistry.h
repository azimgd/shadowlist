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
  SLComponent(std::string uniqueId) : uniqueId(uniqueId), isVisible(false) {}
  std::string getUniqueId() const;
  bool getVisible() const;
  void setVisible(bool visible);

  bool operator==(const SLComponent& other) const {
    return uniqueId == other.uniqueId;
  }

private:
  std::string uniqueId;
  bool isVisible;
};

class SLComponentRegistry {
public:
  using SLObserver = std::function<void(std::string uniqueId, bool isVisible)>;

  SLComponentRegistry();
  void registerComponent(std::string uniqueId);
  void unregisterComponent(std::string uniqueId);
  void mount(const std::vector<std::string> &uniqueIds);
  void unmount(const std::vector<std::string> &uniqueIds);
  void mountObserver(const SLObserver &observer);
  void unmountObserver(const SLObserver &observer);

private:
  std::unordered_map<std::string, SLComponent> components;
  std::vector<SLObserver> observers;
  int initialNumToRender;

  void updateVisibility(const std::vector<std::string> &uniqueIds, bool visible);
  void notifyObservers(const std::unordered_set<SLComponent> &updatedComponents, bool isVisible);
};

namespace std {
  template <>
  struct hash<SLComponent> {
    size_t operator()(const SLComponent& component) const {
      return hash<std::string>()(component.getUniqueId());
    }
  };
}

#endif
