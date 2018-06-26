#ifndef UNISPARKS_REGISTRY_HPP
#define UNISPARKS_REGISTRY_HPP
#include "unisparks/util/meta.hpp"

namespace unisparks {

namespace internal {
struct Deleter {
  virtual ~Deleter();
  Deleter* next = nullptr;
};
extern Deleter* firstDeleter;
template <typename T> struct GenDeleter : Deleter {
  ~GenDeleter() override {
    delete item;
  }
  T* item = nullptr;
};
} // namespace internal

template <typename T, typename... Args>
T& make(Args&& ... args) {
  using namespace internal;
  GenDeleter<T>* d = new GenDeleter<T>;
  d->item = new T(forward<Args>(args)...);
  d->next = firstDeleter;
  firstDeleter = d;
  return *d->item;
}

template <typename T>
T& clone(const T& v) {
  return make<T>(v);
}


} // namespace unisparks
#endif /* UNISPARKS_REGISTRY_HPP */
