#pragma once

#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Row keys in order, read only: a vector's strings, or pointers to keys held elsewhere, the way
 * the key store shares keys between versions. It never owns the keys, which must outlive it.
 */
class KeyList final {
public:
  class Iterator;

  KeyList() = default;
  KeyList(const std::vector<std::string>& keys) : strings_(keys.data()), size_(keys.size()) {}
  KeyList(const std::string* const* keys, std::size_t size) : refs_(keys), size_(size) {}

  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  const std::string& operator[](std::size_t index) const {
    return refs_ != nullptr ? *refs_[index] : strings_[index];
  }
  const std::string& front() const { return (*this)[0]; }
  const std::string& back() const { return (*this)[size_ - 1]; }

  Iterator begin() const;
  Iterator end() const;

  /*
   * Calls visit with a function from an index to its key, made for how this list holds its keys.
   * A loop over many keys inside visit checks that once instead of at every key.
   */
  template <typename Visit>
  decltype(auto) visit(Visit&& visit) const {
    if (refs_ != nullptr) {
      const std::string* const* refs = refs_;
      return visit([refs](std::size_t index) -> const std::string& { return *refs[index]; });
    }
    const std::string* strings = strings_;
    return visit([strings](std::size_t index) -> const std::string& { return strings[index]; });
  }

  friend bool operator==(const KeyList& lhs, const KeyList& rhs) {
    if (lhs.size_ != rhs.size_) {
      return false;
    }
    for (std::size_t index = 0; index < lhs.size_; ++index) {
      if (lhs[index] != rhs[index]) {
        return false;
      }
    }
    return true;
  }
  friend bool operator!=(const KeyList& lhs, const KeyList& rhs) { return !(lhs == rhs); }

private:
  const std::string* strings_ = nullptr;
  const std::string* const* refs_ = nullptr;
  std::size_t size_ = 0;
};

class KeyList::Iterator final {
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = std::string;
  using difference_type = std::ptrdiff_t;
  using pointer = const std::string*;
  using reference = const std::string&;

  Iterator() = default;
  Iterator(KeyList list, std::size_t index) : list_(list), index_(index) {}

  reference operator*() const { return list_[index_]; }
  pointer operator->() const { return &list_[index_]; }
  reference operator[](difference_type offset) const { return list_[index_ + static_cast<std::size_t>(offset)]; }

  Iterator& operator++() {
    ++index_;
    return *this;
  }
  Iterator operator++(int) {
    Iterator previous = *this;
    ++index_;
    return previous;
  }
  Iterator& operator--() {
    --index_;
    return *this;
  }
  Iterator operator--(int) {
    Iterator previous = *this;
    --index_;
    return previous;
  }
  Iterator& operator+=(difference_type offset) {
    index_ = static_cast<std::size_t>(static_cast<difference_type>(index_) + offset);
    return *this;
  }
  Iterator& operator-=(difference_type offset) { return *this += -offset; }
  friend Iterator operator+(Iterator iterator, difference_type offset) { return iterator += offset; }
  friend Iterator operator+(difference_type offset, Iterator iterator) { return iterator += offset; }
  friend Iterator operator-(Iterator iterator, difference_type offset) { return iterator -= offset; }
  friend difference_type operator-(const Iterator& lhs, const Iterator& rhs) {
    return static_cast<difference_type>(lhs.index_) - static_cast<difference_type>(rhs.index_);
  }
  friend bool operator==(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ == rhs.index_; }
  friend bool operator!=(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ != rhs.index_; }
  friend bool operator<(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ < rhs.index_; }
  friend bool operator>(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ > rhs.index_; }
  friend bool operator<=(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ <= rhs.index_; }
  friend bool operator>=(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ >= rhs.index_; }

private:
  KeyList list_;
  std::size_t index_ = 0;
};

inline KeyList::Iterator KeyList::begin() const {
  return {*this, 0};
}

inline KeyList::Iterator KeyList::end() const {
  return {*this, size_};
}

}
