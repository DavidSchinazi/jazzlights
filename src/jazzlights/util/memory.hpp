#ifndef JAZZLIGHTS_UTIL_MEMORY_HPP
#define JAZZLIGHTS_UTIL_MEMORY_HPP
#include <stdlib.h>
#include <stdint.h>
#include "jazzlights/util/meta.hpp"

// <new> header is not available on all boards, so we'll
// declare placement new/delete operators here
#ifdef ARDUINO_AVR_UNO
inline void* operator new(size_t, void* p) noexcept {return p;}
inline void* operator new[](size_t, void* p) noexcept {return p;}
inline void operator delete(void*, void*) noexcept {}
inline void operator delete[](void*, void*) noexcept {}
#else
#include <new>
#endif

// Macro used to simplify the task of deleting all of the default copy
// constructors and assignment operators.
#define DISALLOW_COPY_ASSIGN_AND_MOVE(_class_name)       \
    _class_name(const _class_name&) = delete;            \
    _class_name(_class_name&&) = delete;                 \
    _class_name& operator=(const _class_name&) = delete; \
    _class_name& operator=(_class_name&&) = delete
// Macro used to simplify the task of deleting the non rvalue reference copy
// constructors and assignment operators.  (IOW - forcing move semantics)
#define DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(_class_name) \
    _class_name(const _class_name&) = delete;            \
    _class_name& operator=(const _class_name&) = delete

namespace jazzlights {

// This is a simplified version of std::unique_ptr<> that can automatically
// dispose a pointer when it goes out of scope.
//
// Compared to std::unique_ptr, it does not support custom deleters and has
// restrictive type conversion semantics.
//
// Based on the code in FBL library of Zircon project,
// see https://fuchsia.googlesource.com/zircon/+/master/LICENSE
//
template <typename T>
class unique_ptr {
public:
    constexpr unique_ptr() : ptr_(nullptr) {}
    constexpr unique_ptr(decltype(nullptr)) : unique_ptr() {}
    explicit unique_ptr(T* t) : ptr_(t) { }
    ~unique_ptr() {
        delete(ptr_);
    }
    unique_ptr(unique_ptr&& o) : ptr_(o.release()) {}

    template<typename B>
    unique_ptr(unique_ptr<B>&& o) : ptr_(o.release()) {}

    unique_ptr& operator=(unique_ptr&& o) {
        reset(o.release());
        return *this;
    }
    unique_ptr& operator=(decltype(nullptr)) {
        reset();
        return *this;
    }
    // Comparison against nullptr operators (of the form, myptr == nullptr).
    bool operator==(decltype(nullptr)) const { return (ptr_ == nullptr); }
    bool operator!=(decltype(nullptr)) const { return (ptr_ != nullptr); }
    // Comparison against other unique_ptr<>'s.
    bool operator==(const unique_ptr& o) const { return ptr_ == o.ptr_; }
    bool operator!=(const unique_ptr& o) const { return ptr_ != o.ptr_; }
    bool operator< (const unique_ptr& o) const { return ptr_ <  o.ptr_; }
    bool operator<=(const unique_ptr& o) const { return ptr_ <= o.ptr_; }
    bool operator> (const unique_ptr& o) const { return ptr_ >  o.ptr_; }
    bool operator>=(const unique_ptr& o) const { return ptr_ >= o.ptr_; }
    
    // move semantics only
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(unique_ptr);
    
    T* release() {
        T* t = ptr_;
        ptr_ = nullptr;
        return t;
    }
    void reset(T* t = nullptr) {
        delete ptr_;
        ptr_ = t;
    }
    void swap(unique_ptr& other) {
        T* t = ptr_;
        ptr_ = other.ptr_;
        other.ptr_ = t;
    }
    T* get() const {
        return ptr_;
    }
    explicit operator bool() const {
        return static_cast<bool>(ptr_);
    }
    T& operator*() const {
        return *ptr_;
    }
    T* operator->() const {
        return ptr_;
    }
    
private:
    T* ptr_;
};

template <typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(forward<Args>(args)...));
}

}  // namespace jazzlights
#endif  // JAZZLIGHTS_UTIL_MEMORY_HPP
