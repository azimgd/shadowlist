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
  SLComponent(int componentId) : componentId(componentId), isVisible(false) {}
  int getComponentId() const;
  bool getVisible() const;
  void setVisible(bool visible);

private:
  int componentId;
  bool isVisible;
};

class SLComponentRegistry {
public:
  using SLObserver = std::function<void(int id, bool isVisible)>;

  SLComponentRegistry();
  void registerComponent(int componentId);
  void unregisterComponent(int componentId);
  void mountRange(int visibleStartIndex, int visibleEndIndex);
  void mount(const std::vector<int>& indices);
  void unmount(const std::vector<int>& indices);
  void mountObserver(const SLObserver &observer);
  void unmountObserver(const SLObserver &observer);

private:
  std::unordered_map<int, SLComponent> components;
  std::vector<SLObserver> observers;
  int initialNumToRender;

  void updateVisibility(const std::vector<int>& indices, bool visible);
  void notifyObservers(const std::unordered_set<int>& updatedComponents, bool isVisible);
};

#endif
