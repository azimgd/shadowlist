#include "SLComponentRegistry.h"

std::vector<int> generateIndicesFromRange(int visibleStartIndex, int visibleEndIndex) {
  std::vector<int> indices;

  if (visibleEndIndex <= visibleStartIndex) {
    return indices;
  }
  indices.reserve(visibleEndIndex - visibleStartIndex);
  for (int visibleIndex = visibleStartIndex; visibleIndex < visibleEndIndex; ++visibleIndex) {
    indices.push_back(static_cast<int>(visibleIndex));
  }

  return indices;
}

int SLComponent::getComponentId() const {
  return componentId;
}

bool SLComponent::getVisible() const {
  return isVisible;
}

void SLComponent::setVisible(bool visibleState) {
  isVisible = visibleState;
}

SLComponentRegistry::SLComponentRegistry(int initialNumToRender) : initialNumToRender(initialNumToRender) {}

void SLComponentRegistry::registerComponent(int componentId) {
  components.emplace(componentId, SLComponent{componentId});
}

void SLComponentRegistry::unregisterComponent(int componentId) {
  auto componentIter = components.find(componentId);

  if (componentIter != components.end()) {
    notifyObservers({componentIter->second.getComponentId()}, false);
    components.erase(componentIter);
  }
}

void SLComponentRegistry::mountRange(int visibleStartIndex, int visibleEndIndex) {
  mount(generateIndicesFromRange(visibleStartIndex, visibleEndIndex));
}

void SLComponentRegistry::mount(const std::vector<int>& componentIndices) {
  std::unordered_set<int> currentlyVisible;

  // Collect currently visible components
  for (const auto& componentPair : components) {
    const SLComponent& component = componentPair.second;
    if (component.getVisible()) {
      currentlyVisible.insert(component.getComponentId());
    }
  }

  // Unmount components that are visible but not in the new indices
  for (const auto& visibleId : currentlyVisible) {
    if (std::find(componentIndices.begin(), componentIndices.end(), visibleId) == componentIndices.end()) {
      auto componentIter = components.find(visibleId);
      if (componentIter != components.end()) {
        componentIter->second.setVisible(false);
        notifyObservers({componentIter->first}, false);
      }
    }
  }

  // Mount components specified by indices
  for (int i = 0; i < componentIndices.size(); ++i) {
    int componentId = componentIndices[i];
    auto componentIter = components.find(componentId);
    if (componentIter != components.end()) {
      SLComponent& component = componentIter->second;
      if (i < initialNumToRender) {
        component.setVisible(true);
      } else {
        component.setVisible(false);
      }
      notifyObservers({component.getComponentId()}, component.getVisible());
    }
  }
}

void SLComponentRegistry::mountObserver(const SLObserver& observerFunction) {
  observers.push_back(observerFunction);
}

void SLComponentRegistry::unmount(const std::vector<int>& componentIndices) {
  for (int componentId : componentIndices) {
    auto componentIter = components.find(componentId);
    if (componentIter != components.end()) {
      componentIter->second.setVisible(false);
      notifyObservers({componentIter->first}, false);
    }
  }
}

void SLComponentRegistry::unmountObserver(const SLObserver& observerFunction) {
  observers.erase(
    std::remove_if(observers.begin(), observers.end(),
      [&observerFunction](const SLObserver& observer) {
        return observer.target<void(int, bool)>() == observerFunction.target<void(int, bool)>();
      }),
    observers.end()
  );
}

void SLComponentRegistry::updateVisibility(const std::vector<int>& componentIndices, bool visibleState) {
  std::unordered_set<int> updatedComponents;

  for (int componentId : componentIndices) {
    auto componentIter = components.find(componentId);
    if (componentIter != components.end()) {
      SLComponent& component = componentIter->second;
      if (component.getVisible() != visibleState) {
        component.setVisible(visibleState);
        updatedComponents.insert(component.getComponentId());
      }
    }
  }

  notifyObservers(updatedComponents, visibleState);
}

void SLComponentRegistry::notifyObservers(const std::unordered_set<int>& componentIds, bool visibleState) {
  for (int componentId : componentIds) {
    for (const auto& observerFunction : observers) {
      observerFunction(componentId, visibleState);
    }
  }
}
