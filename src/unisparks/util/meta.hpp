// Template meta-programming helpers
//
// Based on the code in FBL library of Zircon/Fuchsia project,
// see https://fuchsia.googlesource.com/zircon/+/master/LICENSE
#ifndef UNISPARKS_TYPE_SUPPORT_H
#define UNISPARKS_TYPE_SUPPORT_H

namespace unisparks {

template<typename T>
struct iterator_traits {
  using value_type = typename T::value_type;
  using reference = typename T::reference_type;
};

template<typename T>
struct iterator_traits<T*> {
  using value_type = T;
  using reference = T&;
};

template <typename T>
struct function_traits;

template<class R, class... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)> {
};

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {
};

template<class R, class... Args>
struct function_traits<R(Args...)> {
  using return_type = R;
  static constexpr size_t arity = sizeof...(Args);
};

template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const> {
  using return_type = R;
  static constexpr size_t arity = sizeof...(Args);
};

template <typename T, T v>
struct integral_constant {
  static constexpr T value = v;
  using value_type = T;
  using type = integral_constant<T, v>;
};
using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

// is_const:
template <typename T>
struct is_const : false_type {};
template <typename T>
struct is_const<const T> : true_type {};

// is_lvalue_reference:
template <typename T>
struct is_lvalue_reference : false_type {};
template <typename T>
struct is_lvalue_reference<T&> : true_type {};

// is_rvalue_reference:
template <typename T>
struct is_rvalue_reference : false_type {};
template <typename T>
struct is_rvalue_reference < T&& > : true_type {};

// is_class builtin
template <typename T>
struct is_class : public integral_constant<bool, __is_class(T)> { };

// is_base_of builtin
template <typename Base, typename Derived>
struct is_base_of : public
integral_constant<bool, __is_base_of(Base, Derived)> { };

// remove_reference:
template <typename T>
struct remove_reference {
  using type = T;
};
template <typename T>
struct remove_reference<T&> {
  using type = T;
};
template <typename T>
struct remove_reference < T&& > {
  using type = T;
};

// remove_const:
template <typename T>
struct remove_const {
  typedef T type;
};
template <typename T>
struct remove_const<const T> {
  typedef T type;
};

// remove_volatile:
template <typename T>
struct remove_volatile {
  typedef T type;
};
template <typename T>
struct remove_volatile<volatile T> {
  typedef T type;
};

// remove_cv:
template <typename T>
struct remove_cv {
  typedef typename remove_volatile<typename remove_const<T>::type>::type type;
};

// move
template <typename T>
constexpr typename remove_reference<T>::type&& move(T&& t) {
  return static_cast < typename remove_reference<T>::type && >(t);
}

// forward:
template <typename T>
constexpr T&& forward(typename remove_reference<T>::type& t) {
  return static_cast < T && >(t);
}
template <typename T>
constexpr T&& forward(typename remove_reference<T>::type&& t) {
  static_assert(!is_lvalue_reference<T>::value, "bad fbl::forward call");
  return static_cast < T && >(t);
}

// is_same:
template<class T, class U> struct is_same : false_type {};
template<class T> struct is_same<T, T> : true_type {};
// enable_if:
template<bool B, class T = void> struct enable_if { };
template<class T> struct enable_if<true, T> {
  typedef T type;
};


}  // namespace unisparks
#endif /* UNISPARKS_TYPE_SUPPORT_H */
