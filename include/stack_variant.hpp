#ifndef STACK_VARIANT_HPP_GAURD
#define STACK_VARIANT_HPP_GAURD

#include "typelist.hpp"
#include "utility.hpp"
#include <type_traits>

namespace utility {

template<typename... Ts>
struct alignas(typelist<Ts...>::alignof_max) span
{
  char data[typelist<Ts...>::sizeof_max];
  template<typename T>
  constexpr span<Ts...>& operator=(const T& value)
  {
    const_cast<T&>(*this) = value;
  }
  template<typename T>
  constexpr operator const T&() const
  {
    return const_cast<const T&>(*this);
  }
};

template<typename T>
struct wrapper
{
  using type = T;

  constexpr wrapper<T>& operator==(const wrapper<T>&) const { return true; }
  template<typename U>
  constexpr wrapper<T>& operator==(const wrapper<U>&) const
  {
    return false;
  }
  template<typename U>
  constexpr wrapper<T>& operator!=(U&) const
  {
    return !(*this == U{});
  }
};

template<typename T, typename... Ts>
struct stack_variant
{
  struct bad_variant_access : std::runtime_error
  {
    bad_variant_access(const char* what)
      : std::runtime_error(what)
    {}
  };

  using types = typelist<T, Ts...>;
  using value_type = span<T, Ts...>;
  static constexpr size_t invalid_index = sizeof...(Ts) + 1; // one past the last type

  stack_variant()
    : union_(value_type())
    , held_(invalid_index)
  {}
  template<typename I>
  stack_variant(I&& Init)
  {
    if constexpr (tl_has_type<I, types>::value) {
      if constexpr (std::is_lvalue_reference<I>::value) {
        union_.template access<std::remove_reference_t<I>*>() = &Init;
        held_ = tl_index_of<I, types>::index;
      } else {
        union_.template access<I>() = Init;
        held_ = tl_index_of<I, types>::index;
      }
    } else if constexpr (tl_has_conversion<I, types>::value) {
      union_.template access<typename tl_find_conversion<I, types>::type>() = Init;
      held_ = tl_index_of<typename tl_find_conversion<I, types>::type, types>::index;
    } else {
      static_assert(tl_has_type<I, types>::value || tl_has_conversion<I, types>::value,
                    "could not find suitable conversion in variant assignment");
    }
  }

  bool empty() const { return held_ == invalid_index; }

  void clear()
  {
    if (!empty()) {
      union_ = value_type();
      held_ = invalid_index;
    }
  }

  template<typename C>
  bool is_type() const
  {
    return tl_index_of<C, types>::index == held_;
  }

  template<typename P>
  stack_variant<T, Ts...>& operator=(P&& rhs)
  {
    if constexpr (tl_has_type<P, types>::value) {
      if (tl_index_of<P, types>::index == held_) // dont destroy union_ if unneccesary
      {
        if constexpr (std::is_lvalue_reference<P>::value) {
          union_.template access<std::remove_reference_t<P>*>() = &rhs;
        } else {
          union_.template access<P>() = rhs;
        }
        return *this;
      }
      clear();
      if constexpr (std::is_lvalue_reference<P>::value) {
        union_.template access<std::remove_reference_t<P>*>() = &rhs;
        held_ = tl_index_of<P, types>::index;
      } else {
        union_.template access<P>() = rhs;
        held_ = tl_index_of<P, types>::index;
      }
    } else if constexpr (tl_has_conversion<P, types>::value) {
      clear();
      union_.template access<typename tl_find_conversion<P, types>::type>() = rhs;
      held_ = tl_index_of<typename tl_find_conversion<P, types>::type, types>::index;
    } else {
      static_assert(tl_has_type<P, types>::value || tl_has_conversion<P, types>::value,
                    "could not find suitable conversion in variant assignment");
    }
    return *this;
  }

  template<typename G>
  G& get()
  {
    if (!empty()) {
      if (held_ == tl_index_of<G, types>::index) {
        if constexpr (std::is_lvalue_reference<G>::value) {
          return *union_.template access<std::remove_reference_t<G>*>();
        } else {
          return union_.template access<G>();
        }
      } else
        throw bad_variant_access("variant does not hold the requested type");

    } else
      throw bad_variant_access("variant is empty");
  }

  template<typename G>
  const G& get() const
  {
    if (!empty()) {
      if (held_ == tl_index_of<G, types>::index) {
        if constexpr (std::is_lvalue_reference<G>::value) {
          return *union_.template access<std::remove_reference_t<G>*>();
        } else {
          return union_.template access<G>();
        }
      } else
        throw bad_variant_access("variant does not hold the requested type");

    } else
      throw bad_variant_access("variant is empty");
  }

private:
  value_type union_;
  size_t held_;
};
}

#endif