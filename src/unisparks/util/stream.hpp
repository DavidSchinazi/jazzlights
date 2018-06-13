#ifndef UNISPARKS_STREAM_H
#define UNISPARKS_STREAM_H
#include "unisparks/util/meta.hpp"

namespace unisparks {

template<typename T>
struct InputStream  {
  virtual bool read(T& v) = 0;
  virtual void reset() = 0;

  class Iterator {
   public:
    using value_type = T;

    Iterator(InputStream* s) : s_(s) {
      next();
    }

    const T& operator*() {
      return v_;
    }

    bool operator==(const Iterator& other) const {
      return s_ == other.s_;
    }

    bool operator!=(const Iterator& other) {
      return !(*this == other);
    }

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
      if (s_ && !s_->read(v_)) {
        s_ = nullptr;
      }
    }

    T v_ = T();
    InputStream* s_;
  };

  Iterator begin() {
    return Iterator(this);
  }

  Iterator end() {
    return Iterator(nullptr);
  }
};

template<typename Iterator>
class RangeInputStream : public
  InputStream<typename iterator_traits<Iterator>::value_type> {
 public:

  RangeInputStream(Iterator begin, Iterator end) : begin_(begin), end_(end),
    ptr_(begin) {
  }

  bool read(typename iterator_traits<Iterator>::reference v) override {
    if (ptr_ == end_) {
      return false;
    }
    v = *ptr_;
    ptr_++;
    return true;
  }

  void reset() override {
    ptr_ = begin_;
  }

  Iterator begin_;
  Iterator end_;
  Iterator ptr_;
};

template<typename StreamT, typename FuncT, typename RetT = typename function_traits<FuncT>::return_type>
struct MappedInputStream : InputStream<RetT> {
  MappedInputStream(StreamT& s, const FuncT& f) : s(s), f(f) {
  }

  bool read(RetT& v) override {
    typename StreamT::Iterator::value_type vv;
    if (!s.read(vv)) {
      return false;
    }
    v = f(vv);
    return true;
  }

  void reset() override {
    s.reset();
  }

  StreamT& s;
  FuncT f;
};

template<typename StreamT, typename FuncT>
MappedInputStream<StreamT, FuncT> map(StreamT& stream, const FuncT& func) {
  return MappedInputStream<StreamT, FuncT>(stream, func);
}

template<typename Stream1T, typename Stream2T, typename FuncT,
         typename RetT = typename function_traits<FuncT>::return_type>
struct CombinedInputStream : InputStream<RetT> {
  CombinedInputStream(Stream1T& s1, Stream2T& s2,
                      const FuncT& combinator) : s1(s1), s2(s2), f(combinator) {
  }

  bool read(RetT& v) override {
    typename Stream1T::Iterator::value_type v1;
    typename Stream2T::Iterator::value_type v2;
    if (!s1.read(v1)) {
      return false;
    }
    if (!s2.read(v2)) {
      return false;
    }
    v = f(v1, v2);
    return true;
  }

  void reset() override {
    s1.reset();
    s2.reset();
  }

  Stream1T& s1;
  Stream2T& s2;
  FuncT f;
};


} // namespace unisparks
#endif /* UNISPARKS_STREAM_H */
