/* -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Defines a circular vector.

#ifndef DECOMPRESSOR_SRC_UTILS_CIRCULAR_VECTOR_H
#define DECOMPRESSOR_SRC_UTILS_CIRCULAR_VECTOR_H

#include <cassert>
#include <iterator>
#include <vector>

namespace wasm {

namespace utils {

// WARNING: May not destroy popped elements during the pop. However,
// the elements will eventually get destroyed (no later than destruction).
// ToDO(karlschimpf): Emscriptem doen't appear to like the allocator arguments,
// hence they have been removed.
template <class T>
class circular_vector {
 public:
  typedef T value_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef size_t size_type;

  class circ_iterator : public std::bidirectional_iterator_tag {
   public:
    circ_iterator() : circ_vector(nullptr), index(0) {}

    circ_iterator(const circ_iterator& i)
        : circ_vector(i.circ_vector), index(i.index) {}

    circ_iterator(circular_vector* v, size_t index)
        : circ_vector(v), index(index) {}

    ~circ_iterator() {}

    circ_iterator& operator=(const circ_iterator& i) {
      circ_vector = i.circ_vector;
      index = i.index;
    }

    bool operator==(const circ_iterator& i) const {
      return circ_vector == i.circ_vector && index == i.index;
    }

    bool operator!=(const circ_iterator& i) const {
      return circ_vector != i.circ_vector || index != i.index;
    }

    reference operator*() {
      assert(circ_vector != nullptr);
      return (*circ_vector)[index];
    }

    const_reference operator*() const {
      assert(circ_vector != nullptr);
      return (*circ_vector)[index];
    }

    reference operator->() {
      assert(circ_vector != nullptr);
      return (*circ_vector)[index];
    }

    const_reference operator->() const {
      assert(circ_vector != nullptr);
      return (*circ_vector)[index];
    }

    value_type* operator++() {
      assert(circ_vector != nullptr);
      return &(*circ_vector)[++index];
    }

    value_type* operator++(int) {
      assert(circ_vector != nullptr);
      return *(*circ_vector)[index++];
    }

    value_type* operator--() {
      assert(circ_vector != nullptr);
      return &(*circ_vector)[--index];
    }

    value_type* operator--(int) {
      assert(circ_vector != nullptr);
      return &(*circ_vector)[index--];
    }

   private:
    circular_vector* circ_vector;
    size_t index;
  };

  typedef circ_iterator iterator;
  typedef const circ_iterator const_iterator;
  typedef circ_iterator reverse_iterator;
  typedef const circ_iterator const_reverse_iterator;

  explicit circular_vector(size_type vector_max_size)
      : vector_max_size(vector_max_size), start_index(0), vector_size(0) {
    prefill();
  }

  circular_vector(const circular_vector& cv)
      : contents(cv.contents),
        vector_max_size(cv.vector_max_size),
        start_index(cv.start_index),
        vector_size(cv.vector_size) {}

  ~circular_vector() {}

  size_type size() const { return vector_size; }

  size_type max_size() const { return vector_max_size; }

  size_type capacity() const { return contents.capacity(); }

  bool empty() const { return vector_size == 0; }

  bool full() const { return vector_size == vector_max_size; }

  iterator begin() { return circ_iterator(this, 0); }

  const_iterator begin() const {
    return circ_iterator(const_cast<circular_vector*>(this), 0);
  }

  iterator end() { return circ_iterator(this, size()); }

  const_iterator end() const {
    return circ_iterator(const_cast<circular_vector*>(this), size());
  }

  reverse_iterator rbegin() { return circ_iterator(this, size() - 1); }

  const_reverse_iterator rbegin() const {
    return circ_iterator(const_cast<circular_vector*>(this), size() - 1);
  }

  reverse_iterator rend() { return circ_iterator(this, size_type(-1)); }

  const_reverse_iterator rend() const {
    return circ_iterator(const_cast<circular_vector*>(this), size_type(-1));
  }

  reference operator[](size_type n) { return contents.at(get_index(n)); }

  const_reference operator[](size_type n) const {
    return contents.at(get_index(n));
  }

  reference at(size_type n) { return contents.at(get_index(n)); }

  const_reference at(size_type n) const { return contents.at(get_index(n)); }

  reference front() { return at(0); }

  const_reference front() const { return at(0); }

  reference back() { return at(vector_size - 1); }

  const_reference back() const { return at(vector_size - 1); }

  void push_front(const value_type& v) {
    if (vector_size < vector_max_size)
      ++vector_size;
    dec_start_index();
    at(0) = v;
    return;
  }

  void push_back(const value_type& v) {
    if (vector_size < vector_max_size) {
      at(vector_size++) = v;
      return;
    }
    at(0) = v;
    inc_start_index();
    return;
  }

  void pop_front() {
    assert(vector_size > 0);
    --vector_size;
    inc_start_index();
  }

  void pop_back() {
    assert(vector_size > 0);
    --vector_size;
  }

  void swap(circular_vector& cv) {
    contents.swap(cv.contents);
    std::swap(vector_max_size, cv.vector_max_size);
    std::swap(start_index, cv.start_index);
    std::swap(vector_size, cv.vector_size);
  }

  void clear() {
    contents.clear();
    start_index = 0;
    vector_size = 0;
  }

  void resize(size_type new_size) {
    assert(contents.size() == 0);
    clear();
    vector_max_size = new_size;
    prefill();
  }

  // Note: This operation is provided to make debugging easier.
  void describe(FILE* Out, std::function<void(FILE*, T)> describe_fcn) {
    fprintf(Out, "*** circular vector[%" PRIuMAX "] ***\n", uintmax_t(size()));
    for (size_t i = 0; i < size(); ++i)
      describe_fcn(Out, at(i));
    fprintf(Out, "******\n");
  }

 private:
  std::vector<T> contents;
  size_type vector_max_size;
  size_type start_index;
  size_type vector_size;
  size_type get_index(size_type n) const {
    return (start_index + n) % vector_max_size;
  }
  void inc_start_index() { start_index = (start_index + 1) % vector_max_size; }
  void dec_start_index() {
    if (start_index == 0)
      start_index = vector_max_size - 1;
    else
      --start_index;
  }
  void prefill() {
    contents.clear();
    for (size_t i = 0; i < vector_max_size; ++i)
      contents.push_back(T());
  }
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_UTILS_CIRCULAR_VECTOR_H
