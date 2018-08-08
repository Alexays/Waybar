#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <numeric>
#include <string>
#include <string_view>

namespace waybar::util {

  /// Joins a sequence of strings, separating them using `js`
  template<class StrIterator> // Models InputIterator<std::string>
  std::string join_strings(StrIterator b, StrIterator e, std::string_view js = ", ")
  {
    std::string result;
    std::for_each(b, e, [&](auto&& s) {
      if (!result.empty()) {
        result.append(js);
      }
      result.append(s);
    });
    return result;
  }

  inline const char* nonull(const char* str) {
    if (str == nullptr) return "";
    return str;
  };

  inline bool iequals(std::string_view a, std::string_view b)
  {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](char a, char b) { return tolower(a) == tolower(b); });
  }

  inline bool starts_with(std::string_view prefix, std::string_view a)
  {
    return a.compare(0, prefix.size(), prefix) == 0;
  }

  inline bool ends_with(std::string_view prefix, std::string_view a)
  {
    return a.compare(a.size() - prefix.size(), prefix.size(), prefix) == 0;
  }

  /// Return a closure which compares the adress any reference to T to the address of t
  template<typename T>
  constexpr auto addr_eq(T&& t) {
    return [&t] (auto&& t2) {
      return &t == &t2;
    };
  }

  template<typename T>
  bool erase_this(std::vector<T>& cont, T* el)
  {
    if (el < cont.data() && el >= cont.data() + cont.size()) return false;
    cont.erase(cont.begin() + (el - cont.data()));
    return true;
  }

  template<typename T>
  bool erase_this(std::vector<T>& cont, T& el)
  {
    return erase_this(cont, &el);
  }

  namespace detail {
    template<class Func, int... ns>
    constexpr auto generate_array_impl(std::integer_sequence<int, ns...>&&, Func&& gen)
    {
      return std::array<std::decay_t<decltype(std::invoke(gen, std::declval<int>()))>,
                        sizeof...(ns)>{{std::invoke(gen, ns)...}};
    }
  } // namespace detail

  template<int n, class Func>
  constexpr auto generate_array(Func&& gen)
  {
    auto intseq = std::make_integer_sequence<int, n>();
    return detail::generate_array_impl(std::move(intseq), std::forward<Func>(gen));
  }

  namespace view {

    namespace detail {
      template<typename T>
      using store_or_ref_t = std::conditional_t<std::is_rvalue_reference_v<T>, std::decay_t<T>, T&>;
    }

    template<typename Cont>
    struct reverse {

      reverse(Cont&& cont) noexcept : _container(std::forward<Cont>(cont)) {}

      auto begin()
      {
        return std::rbegin(_container);
      }

      auto end()
      {
        return std::rend(_container);
      }

      auto begin() const
      {
        return std::rbegin(_container);
      }

      auto end() const
      {
        return std::rend(_container);
      }

      auto cbegin() const
      {
        return std::crbegin(_container);
      }

      auto cend() const
      {
        return std::crend(_container);
      }

      detail::store_or_ref_t<Cont&&> _container;
    };

    template<typename ContRef>
    reverse(ContRef&& cont) -> reverse<ContRef&&>;

    template<typename Cont>
    struct constant {
      constant(Cont&& cont) noexcept : _container(std::forward<Cont>(cont)){};

      auto begin() const
      {
        return std::cbegin(_container);
      }

      auto end() const
      {
        return std::cend(_container);
      }

      auto cbegin() const
      {
        return std::cbegin(_container);
      }

      auto cend() const
      {
        return std::cend(_container);
      }

      detail::store_or_ref_t<Cont&&> _container;
    };

    template<typename ContRef>
    constant(ContRef&& cont) -> constant<ContRef&&>;

  } // namespace view


  /*
   * Range algorithms
   */

  template<typename InputIt, typename Size, typename F>
  constexpr InputIt for_each_n(InputIt&& first, Size n, F&& f)
  {
    for (Size i = 0; i < n; ++first, ++i) {
      std::invoke(f, *first);
    }
    return first;
  }

  /// `for_each` with access to an index value. Function called as `f(*it, i)`
  ///
  /// For each item in range `[first, last)`, invoke `f` with args
  /// `*iter, i` where `iter` is the current iterator, and `i` is
  /// an incrementing value, starting at zero. Use this instead of
  /// raw indexed loops wherever possible.
  ///
  /// \param first Input iterator to the begining of the range
  /// \param last Input iterator to the end of the range
  /// \param f Must be invocable with arguments `value_type`, `std::size_t`
  /// \returns The number of iterations performed
  template<typename InputIt, typename F>
  constexpr std::size_t indexed_for(InputIt&& first, InputIt&& last, F&& f)
  {
    std::size_t i = 0;
    std::for_each(std::forward<InputIt>(first), std::forward<InputIt>(last), [&](auto&& a) {
      std::invoke(f, a, i);
      i++;
    });
    return i;
  }

  template<typename Rng, typename F>
  constexpr std::size_t indexed_for(Rng&& rng, F&& f)
  {
    return indexed_for(std::begin(rng), std::end(rng), std::forward<F>(f));
  }


  /// `for_each_n` with access to an index value. Function called as `f(*it, i)`
  ///
  /// for `n` iterations, invoke `f` with args `*iter, i`
  /// where `iter` is the current iterator starting with `first`,
  /// and `i` is an incrementing value, starting at zero.
  /// Use this instead of raw indexed loops wherever possible.
  ///
  /// \param first Input iterator to the begining of the range
  /// \param n Number of iterations to go through
  /// \param f Must be invocable with arguments `value_type`, `std::size_t`
  /// \returns An iterator one past the last one visited
  template<class InputIt, class Size, class F>
  constexpr InputIt indexed_for_n(InputIt first, Size n, F&& f)
  {
    for (Size i = 0; i < n; ++first, ++i) {
      std::invoke(f, *first, i);
    }
    return first;
  }

  template<class Rng, class Size, class F>
  constexpr std::size_t indexed_for_n(Rng&& rng, Size n, F&& f)
  {
    return indexed_for_n(std::begin(rng), std::end(rng), n, std::forward<F>(f));
  }

  template<typename Iter1, typename Iter2, typename F>
  constexpr void for_both(Iter1&& f1, Iter1&& l1, Iter2&& f2, Iter2&& l2, F&& f)
  {
    Iter1 i1 = std::forward<Iter1>(f1);
    Iter2 i2 = std::forward<Iter2>(f2);
    for (; i1 != l1 && i2 != l2; i1++, i2++) {
      std::invoke(f, *i1, *i2);
    }
  }

  template<typename Rng1, typename Rng2, typename F>
  constexpr void for_both(Rng1&& r1, Rng2&& r2, F&& f)
  {
    for_both(std::begin(r1), std::end(r1), std::begin(r2), std::end(r2), std::forward<F>(f));
  }

  /*
   * Range based standard algorithms
   *
   * Thanks, chris from SO!
   */

  template<typename Cont, typename T>
  constexpr auto accumulate(Cont&& cont, T&& init)
  {
    // TODO C++20: std::accumulate is constexpr
    using std::begin, std::end;
    auto first = begin(cont);
    auto last = end(cont);
    for (; first != last; ++first) init = init + *first;
    return init;
  }

  template<typename Cont, typename T, typename BinaryOperation>
  constexpr auto accumulate(Cont&& cont, T&& init, BinaryOperation&& op)
  {
    // TODO C++20: std::accumulate is constexpr
    using std::begin, std::end;
    auto first = begin(cont);
    auto last = end(cont);
    for (; first != last; ++first) init = op(init, *first);
    return init;
  }

  template<typename Cont, typename OutputIterator>
  decltype(auto) adjacent_difference(Cont&& cont, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::adjacent_difference(begin(cont), end(cont), std::forward<OutputIterator>(first));
  }

  template<typename Cont>
  decltype(auto) prev_permutation(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::prev_permutation(begin(cont), end(cont));
  }

  template<typename Cont, typename Compare>
  decltype(auto) prev_permutation(Cont&& cont, Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::prev_permutation(begin(cont), end(cont), std::forward<Compare>(comp));
  }

  template<typename Cont>
  decltype(auto) push_heap(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::push_heap(begin(cont), end(cont));
  }

  template<typename Cont, typename Compare>
  decltype(auto) push_heap(Cont&& cont, Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::push_heap(begin(cont), end(cont), std::forward<Compare>(comp));
  }

  template<typename Cont, typename T>
  decltype(auto) remove(Cont&& cont, T&& value)
  {
    using std::begin;
    using std::end;
    return std::remove(begin(cont), end(cont), std::forward<T>(value));
  }

  template<typename Cont, typename OutputIterator, typename T>
  decltype(auto) remove_copy(Cont&& cont, OutputIterator&& first, T&& value)
  {
    using std::begin;
    using std::end;
    return std::remove_copy(begin(cont), end(cont), std::forward<OutputIterator>(first),
                            std::forward<T>(value));
  }

  template<typename Cont, typename OutputIterator, typename UnaryPredicate>
  decltype(auto) remove_copy_if(Cont&& cont, OutputIterator&& first, UnaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::remove_copy_if(begin(cont), end(cont), std::forward<OutputIterator>(first),
                               std::forward<UnaryPredicate>(p));
  }

  template<typename Cont, typename UnaryPredicate>
  decltype(auto) remove_if(Cont&& cont, UnaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::remove_if(begin(cont), end(cont), std::forward<UnaryPredicate>(p));
  }

  template<typename Cont, typename T, typename T2>
  decltype(auto) replace(Cont&& cont, T&& old_value, T2&& new_value)
  {
    using std::begin;
    using std::end;
    return std::replace(begin(cont), end(cont), std::forward<T>(old_value),
                        std::forward<T2>(new_value));
  }

  template<typename Cont, typename OutputIterator, typename T, typename T2>
  decltype(auto) replace_copy(Cont&& cont, OutputIterator&& first, T&& old_value, T2&& new_value)
  {
    using std::begin;
    using std::end;
    return std::replace_copy(begin(cont), end(cont), std::forward<OutputIterator>(first),
                             std::forward<T>(old_value), std::forward<T2>(old_value));
  }

  template<typename Cont, typename OutputIterator, typename UnaryPredicate, typename T>
  decltype(auto) replace_copy_if(Cont&& cont,
                                 OutputIterator&& first,
                                 UnaryPredicate&& p,
                                 T&& new_value)
  {
    using std::begin;
    using std::end;
    return std::replace_copy(begin(cont), end(cont), std::forward<OutputIterator>(first),
                             std::forward<UnaryPredicate>(p), std::forward<T>(new_value));
  }

  template<typename Cont, typename UnaryPredicate, typename T>
  decltype(auto) replace_if(Cont&& cont, UnaryPredicate&& p, T&& new_value)
  {
    using std::begin;
    using std::end;
    return std::replace_if(begin(cont), end(cont), std::forward<UnaryPredicate>(p),
                           std::forward<T>(new_value));
  }

  template<typename Cont>
  decltype(auto) reverse(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::reverse(begin(cont), end(cont));
  }

  template<typename Cont, typename OutputIterator>
  decltype(auto) reverse_copy(Cont&& cont, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::reverse_copy(begin(cont), end(cont), std::forward<OutputIterator>(first));
  }

  template<typename Cont, typename ForwardIterator>
  decltype(auto) rotate(Cont&& cont, ForwardIterator&& new_first)
  {
    using std::begin;
    using std::end;
    return std::rotate(begin(cont), std::forward<ForwardIterator>(new_first), end(cont));
  }

  template<typename Cont, typename ForwardIterator, typename OutputIterator>
  decltype(auto) rotate_copy(Cont&& cont, ForwardIterator&& new_first, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::rotate_copy(begin(cont), std::forward<ForwardIterator>(new_first), end(cont),
                            std::forward<OutputIterator>(first));
  }

  template<typename Cont, typename Cont2>
  decltype(auto) search(Cont&& cont, Cont2&& cont2)
  {
    using std::begin;
    using std::end;
    return std::search(begin(cont), end(cont), begin(cont2), end(cont2));
  }

  template<typename Cont, typename Cont2, typename BinaryPredicate>
  decltype(auto) search(Cont&& cont, Cont2&& cont2, BinaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::search(begin(cont), end(cont), begin(cont2), end(cont2),
                       std::forward<BinaryPredicate>(p));
  }

  template<typename Cont, typename Size, typename T>
  decltype(auto) search_n(Cont&& cont, Size count, T&& value)
  {
    using std::begin;
    using std::end;
    return std::search_n(begin(cont), end(cont), count, std::forward<T>(value));
  }

  template<typename Cont, typename Size, typename T, typename BinaryPredicate>
  decltype(auto) search_n(Cont&& cont, Size count, T&& value, BinaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::search_n(begin(cont), end(cont), count, std::forward<T>(value),
                         std::forward<BinaryPredicate>(p));
  }

  template<typename Cont, typename Cont2, typename OutputIterator>
  decltype(auto) set_difference(Cont&& cont, Cont2&& cont2, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::set_difference(begin(cont), end(cont), begin(cont2), end(cont2),
                               std::forward<OutputIterator>(first));
  }

  template<typename Cont, typename Cont2, typename OutputIterator, typename Compare>
  decltype(auto) set_difference(Cont&& cont, Cont2&& cont2, OutputIterator&& first, Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::set_difference(begin(cont), end(cont), begin(cont2), end(cont2),
                               std::forward<OutputIterator>(first), std::forward<Compare>(comp));
  }

  template<typename Cont, typename Cont2, typename OutputIterator>
  decltype(auto) set_intersection(Cont&& cont, Cont2&& cont2, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::set_intersection(begin(cont), end(cont), begin(cont2), end(cont2),
                                 std::forward<OutputIterator>(first));
  }

  template<typename Cont, typename Cont2, typename OutputIterator, typename Compare>
  decltype(auto) set_intersection(Cont&& cont,
                                  Cont2&& cont2,
                                  OutputIterator&& first,
                                  Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::set_intersection(begin(cont), end(cont), begin(cont2), end(cont2),
                                 std::forward<OutputIterator>(first), std::forward<Compare>(comp));
  }

  template<typename Cont, typename Cont2, typename OutputIterator>
  decltype(auto) set_symmetric_difference(Cont&& cont, Cont2&& cont2, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::set_symmetric_difference(begin(cont), end(cont), begin(cont2), end(cont2),
                                         std::forward<OutputIterator>(first));
  }

  template<typename Cont, typename Cont2, typename OutputIterator, typename Compare>
  decltype(auto) set_symmetric_difference(Cont&& cont,
                                          Cont2&& cont2,
                                          OutputIterator&& first,
                                          Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::set_symmetric_difference(begin(cont), end(cont), begin(cont2), end(cont2),
                                         std::forward<OutputIterator>(first),
                                         std::forward<Compare>(comp));
  }

  template<typename Cont, typename Cont2, typename OutputIterator>
  decltype(auto) set_union(Cont&& cont, Cont2&& cont2, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::set_union(begin(cont), end(cont), begin(cont2), end(cont2),
                          std::forward<OutputIterator>(first));
  }

  template<typename Cont, typename Cont2, typename OutputIterator, typename Compare>
  decltype(auto) set_union(Cont&& cont, Cont2&& cont2, OutputIterator&& first, Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::set_union(begin(cont), end(cont), begin(cont2), end(cont2),
                          std::forward<OutputIterator>(first), std::forward<Compare>(comp));
  }

  template<typename Cont, typename UniformRandomNumberGenerator>
  decltype(auto) shuffle(Cont&& cont, UniformRandomNumberGenerator&& g)
  {
    using std::begin;
    using std::end;
    return std::shuffle(begin(cont), end(cont), std::forward<UniformRandomNumberGenerator>(g));
  }

  template<typename Cont>
  decltype(auto) sort(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::sort(begin(cont), end(cont));
  }

  template<typename Cont, typename Compare>
  decltype(auto) sort(Cont&& cont, Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::sort(begin(cont), end(cont), std::forward<Compare>(comp));
  }

  template<typename Cont>
  decltype(auto) sort_heap(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::sort_heap(begin(cont), end(cont));
  }

  template<typename Cont, typename Compare>
  decltype(auto) sort_heap(Cont&& cont, Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::sort_heap(begin(cont), end(cont), std::forward<Compare>(comp));
  }

  template<typename Cont, typename UnaryPredicate>
  decltype(auto) stable_partition(Cont&& cont, UnaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::stable_partition(begin(cont), end(cont), std::forward<UnaryPredicate>(p));
  }

  template<typename Cont>
  decltype(auto) stable_sort(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::stable_sort(begin(cont), end(cont));
  }

  template<typename Cont, typename Compare>
  decltype(auto) stable_sort(Cont&& cont, Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::stable_sort(begin(cont), end(cont), std::forward<Compare>(comp));
  }

  template<typename Cont, typename ForwardIterator>
  decltype(auto) swap_ranges(Cont&& cont, ForwardIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::swap_ranges(begin(cont), end(cont), std::forward<ForwardIterator>(first));
  }

  template<typename Cont, typename Cont2, typename F>
  auto transform(Cont&& cont, Cont2&& cont2, F&& f) -> decltype(begin(cont2))
  {
    using std::begin;
    using std::end;
    return std::transform(begin(cont), end(cont), begin(cont2), std::forward<F>(f));
  }

  template<typename Cont, typename Iter, typename F>
  decltype(auto) transform(Cont&& cont, Iter&& iter, F&& f)
  {
    using std::begin;
    using std::end;
    return std::transform(begin(cont), end(cont), std::forward<Iter>(iter), std::forward<F>(f));
  }

  template<typename Cont, typename Cont2, typename Cont3, typename BinaryPredicate>
  auto transform(Cont&& cont, Cont2&& cont2, Cont3&& cont3, BinaryPredicate&& f)
    -> decltype(begin(cont2), begin(cont3))
  {
    using std::begin;
    using std::end;
    return std::transform(begin(cont), end(cont), begin(cont2), begin(cont3),
                          std::forward<BinaryPredicate>(f));
  }

  template<typename Cont, typename InputIterator, typename Cont3, typename BinaryPredicate>
  auto transform(Cont&& cont, InputIterator&& iter, Cont3&& cont3, BinaryPredicate&& f)
    -> decltype(begin(cont), begin(cont3))
  {
    using std::begin;
    using std::end;
    return std::transform(begin(cont), end(cont), std::forward<InputIterator>(iter), begin(cont3),
                          std::forward<BinaryPredicate>(f));
  }

  template<typename Cont, typename Cont2, typename InputIterator, typename BinaryPredicate>
  auto transform(Cont&& cont, Cont2&& cont2, InputIterator&& iter, BinaryPredicate&& f)
    -> decltype(begin(cont), begin(cont2), iter)
  {
    using std::begin;
    using std::end;
    return std::transform(begin(cont), end(cont), begin(cont2), std::forward<InputIterator>(iter),
                          std::forward<BinaryPredicate>(f));
  }

  template<typename Cont, typename InputIterator, typename OutputIterator, typename BinaryOperation>
  decltype(auto) transform(Cont&& cont,
                           InputIterator&& firstIn,
                           OutputIterator&& firstOut,
                           BinaryOperation&& op)
  {
    using std::begin;
    using std::end;
    return std::transform(begin(cont), end(cont), std::forward<InputIterator>(firstIn),
                          std::forward<OutputIterator>(firstOut),
                          std::forward<BinaryOperation>(op));
  }

  template<typename Cont>
  decltype(auto) unique(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::unique(begin(cont), end(cont));
  }

  template<typename Cont, typename BinaryPredicate>
  decltype(auto) unique(Cont&& cont, BinaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::unique(begin(cont), end(cont), std::forward<BinaryPredicate>(p));
  }

  template<typename Cont, typename OutputIterator>
  decltype(auto) unique_copy(Cont&& cont, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::unique_copy(begin(cont), end(cont), std::forward<OutputIterator>(first));
  }

  template<typename Cont, typename OutputIterator, typename BinaryPredicate>
  decltype(auto) unique_copy(Cont&& cont, OutputIterator&& first, BinaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::unique_copy(begin(cont), end(cont), std::forward<OutputIterator>(first),
                            std::forward<BinaryPredicate>(p));
  }

  template<typename Cont, typename T>
  decltype(auto) upper_bound(Cont&& cont, T&& value)
  {
    using std::begin;
    using std::end;
    return std::upper_bound(begin(cont), end(cont), std::forward<T>(value));
  }

  template<typename Cont, typename T, typename Compare>
  decltype(auto) upper_bound(Cont&& cont, T&& value, Compare&& comp)
  {
    using std::begin;
    using std::end;
    return std::upper_bound(begin(cont), end(cont), std::forward<T>(value),
                            std::forward<Compare>(comp));
  }

  template<typename Cont, typename OutputIterator>
  decltype(auto) copy(Cont&& cont, OutputIterator&& first)
  {
    using std::begin;
    using std::end;
    return std::copy(begin(cont), end(cont), std::forward<OutputIterator>(first));
  }

  template<typename Cont, typename OutputIterator, typename UnaryPredicate>
  decltype(auto) copy_if(Cont&& cont, OutputIterator&& first, UnaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::copy_if(begin(cont), end(cont), std::forward<OutputIterator>(first),
                        std::forward<UnaryPredicate>(p));
  }

  template<typename Cont, typename T>
  decltype(auto) fill(Cont&& cont, T&& value)
  {
    using std::begin;
    using std::end;
    return std::fill(begin(cont), end(cont), std::forward<T>(value));
  }

  template<typename Cont, typename T>
  decltype(auto) fill_n(Cont&& cont, std::size_t n, T&& value)
  {
    using std::begin;
    using std::end;
    return std::fill_n(begin(cont), n, std::forward<T>(value));
  }

  template<typename Cont, typename UnaryPredicate>
  decltype(auto) any_of(Cont&& cont, UnaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::any_of(begin(cont), end(cont), std::forward<UnaryPredicate>(p));
  }

  template<typename Cont, typename UnaryPredicate>
  decltype(auto) all_of(Cont&& cont, UnaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::all_of(begin(cont), end(cont), std::forward<UnaryPredicate>(p));
  }

  template<typename Cont, typename UnaryPredicate>
  decltype(auto) none_of(Cont&& cont, UnaryPredicate&& p)
  {
    using std::begin;
    using std::end;
    return std::none_of(begin(cont), end(cont), std::forward<UnaryPredicate>(p));
  }

  template<typename Cont>
  decltype(auto) max_element(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::max_element(begin(cont), end(cont));
  }

  template<typename Cont>
  decltype(auto) min_element(Cont&& cont)
  {
    using std::begin;
    using std::end;
    return std::min_element(begin(cont), end(cont));
  }

  template<typename Cont, typename Compare>
  decltype(auto) min_element(Cont&& cont, Compare&& f)
  {
    using std::begin;
    using std::end;
    return std::min_element(begin(cont), end(cont), std::forward<Compare>(f));
  }

  template<typename Cont, typename Compare>
  decltype(auto) max_element(Cont&& cont, Compare&& f)
  {
    using std::begin;
    using std::end;
    return std::max_element(begin(cont), end(cont), std::forward<Compare>(f));
  }

  template<typename Cont, typename T>
  decltype(auto) find(Cont&& cont, T&& t)
  {
    using std::begin;
    using std::end;
    return std::find(begin(cont), end(cont), std::forward<T>(t));
  }

  template<typename Cont, typename UnaryPredicate>
  decltype(auto) find_if(Cont&& cont, UnaryPredicate&& f)
  {
    using std::begin;
    using std::end;
    return std::find_if(begin(cont), end(cont), std::forward<UnaryPredicate>(f));
  }

} // namespace waybar::util
