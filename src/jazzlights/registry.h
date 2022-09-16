#ifndef JAZZLIGHTS_REGISTRY_HPP
#define JAZZLIGHTS_REGISTRY_HPP
#include "jazzlights/util/meta.h"

namespace jazzlights {

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


}  // namespace jazzlights
#endif  // JAZZLIGHTS_REGISTRY_HPP
