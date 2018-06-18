#ifndef UNISPARKS_UTIL_CONTAIN_HPP
#define UNISPARKS_UTIL_CONTAIN_HPP
namespace unisparks {

template<typename T>
struct Slice {
  T* items;
  size_t size;
};

template<typename T>
size_t size(const Slice<T>& c) {
  return c.size;
}

template<typename T>
const T* at(const Slice<T>& c, size_t pos) {
  return c.items[pos];
}

template<typename T>
T* at(Slice<T>& c, size_t pos) {
  return c.items[pos];
}

template<typename T>
const T* begin(const Slice<T>& c) {
  return c.items;
}

template<typename T>
const T* end(const Slice<T>& c) {
  return c.items + c.size;
}

template<typename T>
T* begin(Slice<T>& c) {
  return c.items;
}

template<typename T>
T* end(Slice<T>& c) {
  return c.items + c.size;
}


} // namespace unisparks
#endif /* UNISPARKS_UTIL_CONTAIN_HPP */

