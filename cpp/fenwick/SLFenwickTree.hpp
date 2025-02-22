/*
 * MIT License
 *
 * Copyright (c) 2018 Henry Lee
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#ifndef SLFenwickTree_hpp
#define SLFenwickTree_hpp

#include <vector>

namespace azimgd::shadowlist {

template <class T, class Alloc = std::allocator<T>>
class fenwick {

 private:
  class lvalue_type;
  template<typename Reference> class iterator_type;

 public:
  /*
   * Member types
   */
  typedef T value_type;
  typedef Alloc allocator_type;
  typedef lvalue_type reference;
  typedef const value_type& const_reference;
  typedef typename std::allocator_traits<allocator_type>::const_pointer pointer;
  typedef typename std::allocator_traits<allocator_type>::const_pointer const_pointer;
  typedef iterator_type<reference> iterator;
  typedef iterator_type<const_reference> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef ptrdiff_t difference_type;
  typedef size_t size_type;

  /*
   * Constructors
   */
  fenwick() { clear(); }

  fenwick(size_type size, const value_type& val = value_type()) {
    resize(size, val);
  };

  /*
   * Iterators
   */
  iterator begin() noexcept { return iterator(*this, 0); };
  const_iterator begin() const noexcept { return const_iterator(*this, 0); };

  iterator end() noexcept { return iterator(*this, size_); };
  const_iterator end() const noexcept { return const_iterator(*this, size_); };

  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); };
  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(cend());
  };

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); };
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(cbegin());
  };

  const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  const_reverse_iterator crend() const noexcept { return rend(); }

  /*
   * Capacity
   */
  void resize(size_type size) {
    resize(size, value_type());
  }

  void resize(size_type size, const value_type& val);

  size_type size() const { return size_; }
  size_type capacity() const { return capacity_; }

  /*
   * Search
   */
  size_type lower_bound(value_type target) const;

  /*
   * Sum
   */
  value_type sum(size_type n) const;
  value_type sum(size_type first, size_type last) const;

  /*
   * Element access
   */
  reference operator[](size_type idx) { return lvalue_type(*this, idx); }

  const_reference operator[](size_type idx) const { return data_[idx]; }

  reference at(size_type idx) {
    check_out_of_range(idx);
    return (*this)[idx];
  }

  const_reference at(size_type idx) const {
    check_out_of_range(idx);
    return (*this)[idx];
  }

  reference front() { return operator[](0); }
  const_reference front() const { return operator[](0); }

  reference back() { return operator[](size_ - 1); }
  const_reference back() const { return operator[](size_ - 1); }

  /*
   * Modifiers
   */
  void assign(size_type n, const value_type& val) {
    clear();
    resize(n, val);
  }

  void push_back(const value_type& val) {
    ++size_;

    if (size_ > capacity_) {
      if (capacity_ > 0)
        capacity_ *= 2;
      else
        capacity_++;

      data_.resize(capacity_);
      reallocate_tree(capacity_, value_type());
    } else {
      data_.resize(size_);
    }

    update_delta(size_ - 1, val);
    data_[size_ - 1] = val;
  }

  void clear() noexcept {
    size_ = 0;
    capacity_ = 0;
    data_.clear();
    tree_.clear();
  };

  /*
   * Allocator
   */
  allocator_type get_allocator() const {
    return allocator_type();
  }

 private:
  class lvalue_type {
   public:
    lvalue_type(fenwick& tree, size_type idx) : tree_(tree), idx_(idx) {}

    operator value_type() const { return tree_.data_[idx_]; }

    lvalue_type& operator+=(const_reference delta) {
      tree_.update_delta(idx_, delta);
      tree_.data_[idx_] += delta;
      return *this;
    }

    lvalue_type& operator-=(const_reference delta) {
      tree_.update_delta(idx_, -delta);
      tree_.data_[idx_] -= delta;
      return *this;
    }

    lvalue_type& operator=(const_reference value) {
      tree_.update_value(idx_, value);
      tree_.data_[idx_] = value;
      return *this;
    }

   private:
    fenwick& tree_;
    const size_type idx_;
  };

  template<typename Reference>
  class iterator_type {
   public:
    typedef std::random_access_iterator_tag iterator_category;
    typedef value_type value_type;
    typedef difference_type difference_type;
    typedef Reference reference;
    typedef pointer pointer;

    iterator_type() {}

    reference operator*() const {
      return tree_->at(idx_);
    }

    pointer operator->() const {
      return &(static_cast<const fenwick *>(tree_)->at(idx_));
    }

    iterator_type& operator++() {
      ++idx_;
      return *this;
    }

    iterator_type operator++(int) {
      iterator_type tmp(*this);
      ++idx_;
      return tmp;
    }

    iterator_type& operator--() {
      --idx_;
      return *this;
    }

    iterator_type operator--(int) {
      iterator_type tmp(*this);
      --idx_;
      return tmp;
    }

    template<typename Tp>
    bool operator==(const iterator_type<Tp>& other) {
      return tree_ == other.tree_ && idx_ == other.idx_;
    }

    template<typename Tp>
    bool operator!=(const iterator_type<Tp>& other) {
      return !operator==(other);
    }

   private:
    typedef typename std::conditional<
        std::is_const<typename std::remove_reference<reference>::type>::value,
        const fenwick&, fenwick&>::type tree_reference;
    typedef typename std::conditional<
        std::is_const<typename std::remove_reference<reference>::type>::value,
        const fenwick*, fenwick*>::type tree_pointer;

    tree_pointer tree_;
    size_type idx_;

    iterator_type(tree_reference tree, size_type idx) : tree_(&tree), idx_(idx) {}

    friend class fenwick<T, Alloc>;
  };

  std::vector<value_type, allocator_type> data_;
  std::vector<value_type, allocator_type> tree_;

  size_type size_ = 0;
  size_type capacity_ = 0;

  void update_value(size_type idx, const_reference value);
  void update_delta(size_type idx, const_reference delta);
  void update_recursive(size_type idx, const_reference delta);
  void reallocate_tree(size_type size, const value_type& val);

  void check_out_of_range(size_type idx) const;

  friend class lvalue_type;
};

template<class T, class Alloc>
void fenwick<T, Alloc>::resize(size_type size, const value_type& val) {
  data_.resize(size, val);

  if (size > capacity_) {
    /*
     * If the container is expanding, the tree needs to be recalcuated.
     */
    capacity_ = size;
    reallocate_tree(size, val);
  }

  size_ = size;
}

/*
 * A custom implementation of tree_.resize(size, val)
 */
template<class T, class Alloc>
void fenwick<T, Alloc>::reallocate_tree(size_type size, const value_type& val) {
  tree_.clear();
  tree_.resize(size);
  for (size_type i = 0; i < size_; i++) {
    update_delta(i, data_[i]);
  }
  for (size_type i = size_; i < size; i++) {
    update_delta(i, val);
  }
}

/*
 * A custom implementation of tree_[i] = value
 */
template<class T, class Alloc>
void fenwick<T, Alloc>::update_value(size_type idx, const_reference value) {
  const_reference delta = value - data_[idx];
  update_delta(idx, delta);
}

/*
 * A custom implementation of tree_[i] += delta
 */
template<class T, class Alloc>
void fenwick<T, Alloc>::update_delta(size_type idx, const_reference delta) {
  update_recursive(idx + 1, delta);
}

template<class T, class Alloc>
void fenwick<T, Alloc>::update_recursive(size_type idx, const_reference delta) {
  if (idx > capacity_) {
    return;
  }

  tree_[idx - 1] += delta;

  idx = idx + (idx & (-idx));

  update_recursive(idx, delta);
}

template<class T, class Alloc>
typename fenwick<T, Alloc>::value_type
fenwick<T, Alloc>::sum(size_type n) const {
  value_type ret = 0;

  if (n > size_) {
    n = size_;
  }

  while (n > 0) {
    ret += tree_[n - 1];
    n = n - (n & (-n));
  }

  return ret;
}

template<class T, class Alloc>
typename fenwick<T, Alloc>::value_type
fenwick<T, Alloc>::sum(size_type first, size_type last) const {
  return sum(last) - sum(first);
}

template<class T, class Alloc>
void fenwick<T, Alloc>::check_out_of_range(size_type idx) const {
  if (idx >= size_) {
    throw std::out_of_range("fenwick: index out of range");
  }
}

/*
 * Search for the smallest index i such that sum(i) >= target
 */
template<class T, class Alloc>
typename fenwick<T, Alloc>::size_type
fenwick<T, Alloc>::lower_bound(value_type target) const {
  size_type l = 0;
  size_type r = size_;
  while (l < r) {
    size_type m = l + (r - l) / 2;
    if (sum(m) >= target)
      r = m;
    else
      l = m + 1;
  }
  return l;
}

using SLFenwickTree = fenwick<float>;

}

#endif
