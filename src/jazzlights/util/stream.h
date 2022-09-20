#ifndef JAZZLIGHTS_STREAM_H
#define JAZZLIGHTS_STREAM_H
#include "jazzlights/util/meta.h"

namespace jazzlights {

template <typename T>
struct InputStream {
  virtual bool read(T& v) = 0;
  virtual size_t minSize() const { return 0; }
  virtual size_t maxSize() const { return SIZE_MAX; }

  class Iterator {
   public:
    using value_type = T;

    Iterator(InputStream* s) : s_(s) { next(); }

    const T& operator*() { return v_; }

    bool operator==(const Iterator& other) const { return s_ == other.s_; }

    bool operator!=(const Iterator& other) { return !(*this == other); }

    Iterator& operator++() {
      next();
      return *this;
    }

    Iterator& operator++(int) {
      next();
      return *this;
    }

   private:
    void next() {
      if (s_ && !s_->read(v_)) { s_ = nullptr; }
    }

    T v_ = T();
    InputStream* s_;
  };

  Iterator begin() { return Iterator(this); }

  Iterator end() { return Iterator(nullptr); }
};

template <typename Iterator>
class RangeInputStream : public InputStream<typename iterator_traits<Iterator>::value_type> {
 public:
  RangeInputStream(Iterator begin, Iterator end) : begin_(begin), end_(end), ptr_(begin) {}

  bool read(typename iterator_traits<Iterator>::reference v) override {
    if (ptr_ == end_) { return false; }
    v = *ptr_;
    ptr_++;
    return true;
  }

  size_t minSize() const override { return end_ - ptr_; }

  size_t maxSize() const override { return end_ - ptr_; }

  Iterator begin_;
  Iterator end_;
  Iterator ptr_;
};

template <typename StreamT, typename FuncT, typename RetT = typename function_traits<FuncT>::return_type>
struct MappedInputStream : InputStream<RetT> {
  MappedInputStream(StreamT& s, const FuncT& f) : s(s), f(f) {}

  bool read(RetT& v) override {
    typename StreamT::Iterator::value_type vv;
    if (!s.read(vv)) { return false; }
    v = f(vv);
    return true;
  }

  size_t minSize() const override { return s.minSize(); }

  size_t maxSize() const override { return s.maxSize(); }

  StreamT& s;
  FuncT f;
};

template <typename StreamT, typename FuncT>
MappedInputStream<StreamT, FuncT> map(StreamT& stream, const FuncT& func) {
  return MappedInputStream<StreamT, FuncT>(stream, func);
}

}  // namespace jazzlights
#endif  // JAZZLIGHTS_STREAM_H
