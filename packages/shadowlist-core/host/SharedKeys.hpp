#pragma once

#include <shadowlist-core/KeyList.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Row keys that versions of a list share. Every key is stored once, and each version that keeps
 * it points at the same string. An edit stores only the keys it adds and never copies a key it
 * keeps. A version's pointers sit in a buffer with spare room before and after them: keys added
 * at an end go into that room while no other version has used it, and keys deleted at an end
 * leave the buffer shared as it is. Any other edit copies the pointers. Deleted keys stay stored
 * until they outnumber the kept ones by COMPACT_SLACK, and the next edit then stores the kept
 * keys afresh. A value never changes once made. Reading and splicing are safe from any thread.
 */
class SharedKeys final {
public:
  static constexpr std::size_t COMPACT_SLACK = 1024;

  SharedKeys() = default;
  explicit SharedKeys(std::vector<std::string> keys);
  SharedKeys(const SharedKeys&) = default;
  SharedKeys& operator=(const SharedKeys&) = default;
  SharedKeys(SharedKeys&& other) noexcept;
  SharedKeys& operator=(SharedKeys&& other) noexcept;

  std::size_t size() const { return size_; }
  KeyList list() const { return {slots_ ? slots_->data.get() + begin_ : nullptr, size_}; }

  /*
   * count keys from start, which fall inside.
   */
  KeyList list(std::size_t start, std::size_t count) const {
    return {slots_ ? slots_->data.get() + begin_ + start : nullptr, count};
  }
  const std::string& operator[](std::size_t index) const { return *slots_->data[begin_ + index]; }

  /*
   * These keys with deleted keys from start replaced by inserted. start and deleted fall inside.
   */
  SharedKeys splice(std::size_t start, std::size_t deleted, std::vector<std::string> inserted) const;

  class Builder;

private:
  struct Pool;

  /*
   * Key pointers that versions share. The slots from front to back are in use, and a version
   * claims room past them before it writes there.
   */
  struct Slots {
    explicit Slots(std::size_t capacity) : capacity(capacity), data(new const std::string*[capacity]) {}

    const std::size_t capacity;
    std::unique_ptr<const std::string*[]> data;
    std::atomic<std::size_t> front{0};
    std::atomic<std::size_t> back{0};
  };

  SharedKeys(std::shared_ptr<Pool> pool, std::shared_ptr<Slots> slots, std::size_t begin, std::size_t size);

  bool compactDue(std::size_t size, std::size_t added) const;
  bool extend(std::size_t start, std::vector<std::string>& inserted, SharedKeys& result) const;

  std::shared_ptr<Pool> pool_;
  std::shared_ptr<Slots> slots_;
  std::size_t begin_ = 0;
  std::size_t size_ = 0;
};

/*
 * Keys in order from runs of base's keys and new keys, size of them in all.
 */
class SharedKeys::Builder final {
public:
  Builder(const SharedKeys& base, std::size_t size);

  void append(std::size_t start, std::size_t count);
  void append(std::string key);
  SharedKeys finish();

private:
  SharedKeys base_;
  std::shared_ptr<Slots> slots_;
  std::size_t begin_ = 0;
  std::size_t size_ = 0;
  std::size_t count_ = 0;
  std::vector<std::string> added_;
  std::vector<std::size_t> addedAt_;
};

}
