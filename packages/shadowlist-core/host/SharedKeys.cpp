#include <shadowlist-core/host/SharedKeys.hpp>

#include <algorithm>
#include <cassert>
#include <deque>
#include <mutex>
#include <utility>

namespace azimgd::shadowlist {

namespace {

/*
 * Spare slots on each side of a new buffer for size keys.
 */
std::size_t roomFor(std::size_t size) {
  return size / 4 + 16;
}

}

/*
 * Every key stored for a line of versions. A deque never moves what it holds while it grows.
 */
struct SharedKeys::Pool {
  std::mutex mutex;
  std::deque<std::string> keys;
};

SharedKeys::SharedKeys(std::shared_ptr<Pool> pool, std::shared_ptr<Slots> slots, std::size_t begin, std::size_t size)
  : pool_(std::move(pool)), slots_(std::move(slots)), begin_(begin), size_(size) {}

SharedKeys::SharedKeys(SharedKeys&& other) noexcept
  : pool_(std::move(other.pool_)),
    slots_(std::move(other.slots_)),
    begin_(std::exchange(other.begin_, 0)),
    size_(std::exchange(other.size_, 0)) {}

SharedKeys& SharedKeys::operator=(SharedKeys&& other) noexcept {
  pool_ = std::move(other.pool_);
  slots_ = std::move(other.slots_);
  begin_ = std::exchange(other.begin_, 0);
  size_ = std::exchange(other.size_, 0);
  return *this;
}

SharedKeys::SharedKeys(std::vector<std::string> keys) : size_(keys.size()) {
  if (keys.empty()) {
    return;
  }
  begin_ = roomFor(size_);
  pool_ = std::make_shared<Pool>();
  slots_ = std::make_shared<Slots>(size_ + 2 * begin_);
  for (std::size_t index = 0; index < size_; ++index) {
    pool_->keys.push_back(std::move(keys[index]));
    slots_->data[begin_ + index] = &pool_->keys.back();
  }
  slots_->front = begin_;
  slots_->back = begin_ + size_;
}

bool SharedKeys::compactDue(std::size_t size, std::size_t added) const {
  std::lock_guard<std::mutex> lock(pool_->mutex);
  return pool_->keys.size() + added > 2 * size + COMPACT_SLACK;
}

bool SharedKeys::extend(std::size_t start, std::vector<std::string>& inserted, SharedKeys& result) const {
  std::size_t count = inserted.size();
  std::size_t at = 0;
  if (start == size_) {
    std::size_t end = begin_ + size_;
    if (count > slots_->capacity - end || !slots_->back.compare_exchange_strong(end, end + count)) {
      return false;
    }
    at = begin_ + size_;
  } else if (start == 0) {
    std::size_t first = begin_;
    if (count > first || !slots_->front.compare_exchange_strong(first, first - count)) {
      return false;
    }
    at = begin_ - count;
  } else {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(pool_->mutex);
    for (std::size_t index = 0; index < count; ++index) {
      pool_->keys.push_back(std::move(inserted[index]));
      slots_->data[at + index] = &pool_->keys.back();
    }
  }
  result = SharedKeys(pool_, slots_, start == 0 ? at : begin_, size_ + count);
  return true;
}

SharedKeys SharedKeys::splice(std::size_t start, std::size_t deleted, std::vector<std::string> inserted) const {
  assert(start <= size_ && deleted <= size_ - start);
  std::size_t count = inserted.size();
  if (deleted == 0 && count == 0) {
    return *this;
  }
  std::size_t size = size_ - deleted + count;
  if (pool_ && !compactDue(size, count)) {
    if (count == 0 && (start == 0 || start + deleted == size_)) {
      return SharedKeys(pool_, slots_, start == 0 ? begin_ + deleted : begin_, size);
    }
    SharedKeys result;
    if (deleted == 0 && extend(start, inserted, result)) {
      return result;
    }
  }
  Builder builder(*this, size);
  builder.append(0, start);
  for (std::string& key : inserted) {
    builder.append(std::move(key));
  }
  builder.append(start + deleted, size_ - start - deleted);
  return builder.finish();
}

SharedKeys::Builder::Builder(const SharedKeys& base, std::size_t size)
  : base_(base), slots_(std::make_shared<Slots>(size + 2 * roomFor(size))), begin_(roomFor(size)), size_(size) {}

void SharedKeys::Builder::append(std::size_t start, std::size_t count) {
  assert(count_ + count <= size_ && start + count <= base_.size_);
  if (count == 0) {
    return;
  }
  const std::string* const* from = base_.slots_->data.get() + base_.begin_ + start;
  std::copy(from, from + count, slots_->data.get() + begin_ + count_);
  count_ += count;
}

void SharedKeys::Builder::append(std::string key) {
  assert(count_ < size_);
  addedAt_.push_back(count_++);
  added_.push_back(std::move(key));
}

SharedKeys SharedKeys::Builder::finish() {
  assert(count_ == size_);
  if (size_ == 0) {
    return {};
  }
  bool fresh = !base_.pool_ || base_.compactDue(size_, added_.size());
  std::shared_ptr<Pool> pool = fresh ? std::make_shared<Pool>() : base_.pool_;
  const std::string** rows = slots_->data.get() + begin_;
  {
    std::lock_guard<std::mutex> lock(pool->mutex);
    if (fresh) {
      std::size_t next = 0;
      for (std::size_t row = 0; row < size_; ++row) {
        if (next < addedAt_.size() && addedAt_[next] == row) {
          pool->keys.push_back(std::move(added_[next++]));
        } else {
          pool->keys.push_back(*rows[row]);
        }
        rows[row] = &pool->keys.back();
      }
    } else {
      for (std::size_t index = 0; index < added_.size(); ++index) {
        pool->keys.push_back(std::move(added_[index]));
        rows[addedAt_[index]] = &pool->keys.back();
      }
    }
  }
  slots_->front = begin_;
  slots_->back = begin_ + size_;
  return SharedKeys(std::move(pool), std::move(slots_), begin_, size_);
}

}
