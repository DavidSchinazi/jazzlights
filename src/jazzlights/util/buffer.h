
#ifndef JL_UTIL_BUFFER_H
#define JL_UTIL_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace jazzlights {

// We would normally just use std::string and std::string_view, but we're unfortunately stuck supporting C++11 for now,
// and we don't want to take a dependency on Abseil.

template <typename T>
class OwnedBuffer {
 public:
  explicit OwnedBuffer(size_t size) : buffer_(size, T{}) {}
  T& operator[](size_t position) { return buffer_[position]; }
  const T& operator[](size_t position) const { return buffer_[position]; }
  size_t size() const { return buffer_.size(); }
  void resize(size_t size) { buffer_.resize(size, T{}); }

 private:
  std::vector<T> buffer_;
};

template <typename T>
class BufferView {
 public:
  explicit BufferView(T* data, size_t size) : data_(data), size_(size) {}
  BufferView(OwnedBuffer<T>& ownedBuffer) : data_(&ownedBuffer[0]), size_(ownedBuffer.size()) {}
  explicit BufferView(OwnedBuffer<T>& ownedBuffer, size_t startPosition, size_t endPosition)
      : data_(&ownedBuffer[startPosition]), size_(endPosition - startPosition) {}
  explicit BufferView(OwnedBuffer<T>& ownedBuffer, size_t startPosition)
      : BufferView(ownedBuffer, startPosition, ownedBuffer.size()) {}
  T& operator[](size_t position) { return data_[position]; }
  const T& operator[](size_t position) const { return data_[position]; }
  size_t size() const { return size_; }
  void resize(size_t size) { size_ = size; }

 private:
  T* data_;
  size_t size_;
};

using OwnedBufferU8 = OwnedBuffer<uint8_t>;
using BufferViewU8 = BufferView<uint8_t>;

}  // namespace jazzlights

#endif  // JL_UTIL_BUFFER_H
