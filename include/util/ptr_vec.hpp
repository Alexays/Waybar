#pragma once

#include <cassert>
#include <memory>
#include <type_traits>
#include <vector>
#include "algorithm.hpp"

namespace waybar::util {

  /// An iterator wrapper that dereferences twice.
  template<typename Iter>
  struct double_iterator {
    using wrapped = Iter;

    using value_type = std::decay_t<decltype(*std::declval<typename wrapped::value_type>())>;
    using difference_type = typename wrapped::difference_type;
    using reference = value_type&;
    using pointer = value_type*;
    using iterator_category = std::random_access_iterator_tag;

    using self_t = double_iterator<Iter>;

    double_iterator(wrapped w) : _iter(std::move(w)) {}
    double_iterator() : _iter() {}

    reference operator*() const
    {
      return (**_iter);
    }
    pointer operator->() const
    {
      return &(**_iter);
    }

    self_t& operator++()
    {
      _iter.operator++();
      return *this;
    }
    self_t operator++(int i)
    {
      return _iter.operator++(i);
    }
    self_t& operator--()
    {
      _iter.operator--();
      return *this;
    }
    self_t operator--(int i)
    {
      return _iter.operator--(i);
    }

    auto operator==(const self_t& rhs) const noexcept
    {
      return _iter == rhs._iter;
    }
    auto operator!=(const self_t& rhs) const noexcept
    {
      return _iter != rhs._iter;
    }
    auto operator<(const self_t& rhs) const noexcept
    {
      return _iter < rhs._iter;
    }
    auto operator>(const self_t& rhs) const noexcept
    {
      return _iter > rhs._iter;
    }
    auto operator<=(const self_t& rhs) const noexcept
    {
      return _iter <= rhs._iter;
    }
    auto operator>=(const self_t& rhs) const noexcept
    {
      return _iter >= rhs._iter;
    }

    self_t operator+(difference_type d) const noexcept
    {
      return _iter + d;
    }
    self_t operator-(difference_type d) const noexcept
    {
      return _iter - d;
    }
    auto operator-(const self_t& rhs) const noexcept
    {
      return _iter - rhs._iter;
    }

    self_t& operator+=(difference_type d)
    {
      _iter += d;
      return *this;
    }
    self_t& operator-=(difference_type d)
    {
      _iter -= d;
      return *this;
    }

    operator wrapped&()
    {
      return _iter;
    }
    operator const wrapped&() const
    {
      return _iter;
    }

    wrapped& data()
    {
      return _iter;
    }
    const wrapped& data() const
    {
      return _iter;
    }

  private:
    wrapped _iter;
  };

  template<typename Iter>
  auto operator+(typename double_iterator<Iter>::difference_type diff, double_iterator<Iter> iter)
  {
    return iter + diff;
  }

  /// To avoid clients being moved, they are stored in unique_ptrs, which are
  /// moved around in a vector. This class is purely for convenience, to still
  /// have iterator semantics, and a few other utility functions
  template<typename T>
  struct ptr_vec {
    using value_type = T;

    std::vector<std::unique_ptr<value_type>> _order;

    using iterator = double_iterator<typename decltype(_order)::iterator>;
    using const_iterator = double_iterator<typename decltype(_order)::const_iterator>;

    using reverse_iterator = double_iterator<typename decltype(_order)::reverse_iterator>;
    using const_reverse_iterator =
      double_iterator<typename decltype(_order)::const_reverse_iterator>;

    value_type& push_back(const value_type& v)
    {
      auto ptr = std::make_unique<value_type>(v);
      auto res = ptr.get();
      _order.push_back(std::move(ptr));
      return *res;
    }

    value_type& push_back(value_type&& v)
    {
      auto ptr = std::make_unique<value_type>(std::move(v));
      auto res = ptr.get();
      _order.push_back(std::move(ptr));
      return *res;
    }

    value_type& push_back(std::unique_ptr<value_type> ptr)
    {
      auto res = ptr.get();
      _order.push_back(std::move(ptr));
      return *res;
    }

    template<typename... Args>
    value_type& emplace_back(Args&&... args)
    {
      return push_back(std::make_unique<value_type>(std::forward<Args>(args)...));
    }

    std::unique_ptr<value_type> erase(const value_type& v)
    {
      auto iter =
        std::find_if(_order.begin(), _order.end(), [&v](auto&& uptr) { return uptr.get() == &v; });
      if (iter != _order.end()) {
        auto uptr = std::move(*iter);
        _order.erase(iter);
        return uptr;
      }
      return nullptr;
    }

    iterator rotate_to_back(const value_type& v)
    {
      auto iter =
        std::find_if(_order.begin(), _order.end(), [&v](auto&& uptr) { return uptr.get() == &v; });
      return rotate_to_back(iter);
    }

    iterator rotate_to_back(iterator iter)
    {
      if (iter != _order.end()) {
        {
          return std::rotate(iter.data(), iter.data() + 1, _order.end());
        }
      }
      return end();
    }

    iterator rotate_to_front(const value_type& v)
    {
      auto iter =
        std::find_if(_order.begin(), _order.end(), [&v](auto&& uptr) { return uptr.get() == &v; });
      return rotate_to_front(iter);
    }

    iterator rotate_to_front(iterator iter)
    {
      if (iter != _order.end()) {
        {
          return std::rotate(_order.begin(), iter.data(), iter.data() + 1);
        }
      }
      return end();
    }

    std::size_t size() const noexcept
    {
      return _order.size();
    }

    bool empty() const noexcept
    {
      return _order.empty();
    }

    std::size_t capacity() const noexcept
    {
      return _order.capacity();
    }

    std::size_t max_size() const noexcept
    {
      return _order.max_size();
    }

    void reserve(std::size_t new_cap) 
    {
      _order.reserve(new_cap);
    }

    void shrink_to_fit()
    {
      _order.shrink_to_fit();
    }

    value_type& operator[](std::size_t n)
    {
      return *_order[n];
    }

    const value_type& operator[](std::size_t n) const
    {
      return *_order[n];
    }

    value_type& at(std::size_t n)
    {
      return *_order.at(n);
    }

    const value_type& at(std::size_t n) const
    {
      return *_order.at(n);
    }

    iterator begin()
    {
      return _order.begin();
    }
    iterator end()
    {
      return _order.end();
    }
    const_iterator begin() const
    {
      return _order.begin();
    }
    const_iterator end() const
    {
      return _order.end();
    }

    reverse_iterator rbegin()
    {
      return _order.rbegin();
    }
    reverse_iterator rend()
    {
      return _order.rend();
    }
    const_reverse_iterator rbegin() const
    {
      return _order.rbegin();
    }
    const_reverse_iterator rend() const
    {
      return _order.rend();
    }

    value_type& front()
    {
      return *_order.front();
    }

    value_type& back()
    {
      return *_order.back();
    }

    const value_type& front() const
    {
      return *_order.front();
    }

    const value_type& back() const
    {
      return *_order.back();
    }

    std::vector<std::unique_ptr<value_type>>& underlying() {
      return _order;
    }
  };

  template<typename T, typename T2>
  std::unique_ptr<T> erase_this(ptr_vec<T>& vec, T2* el)
  {
    return vec.erase(*el);
  }

  template<typename T, typename T2>
  std::unique_ptr<T> erase_this(ptr_vec<T>& vec, T2& el)
  {
    return vec.erase(el);
  }

  template<typename T>
  struct non_null_ptr {
    non_null_ptr() = delete;
    constexpr non_null_ptr(T* ptr) : _ptr(ptr)
    {
      assert(ptr != nullptr);
    }
    non_null_ptr(std::nullptr_t) = delete;

    constexpr non_null_ptr(const non_null_ptr&) = default;
    constexpr non_null_ptr(non_null_ptr&&) = default;
    constexpr non_null_ptr& operator=(const non_null_ptr&) = default;
    constexpr non_null_ptr& operator=(non_null_ptr&&) = default;

    constexpr T& operator*() const noexcept
    {
      return *_ptr;
    }

    constexpr T* operator->() const noexcept
    {
      return _ptr;
    }

    constexpr operator T*() noexcept
    {
      return _ptr;
    }

    constexpr operator T* const() const noexcept
    {
      return _ptr;
    }

  private:
    T* _ptr;
  };

  template<typename T>
  struct ref_vec {
    using value_type = T;

    std::vector<value_type*> _order;

    using iterator = double_iterator<typename decltype(_order)::iterator>;
    using const_iterator = double_iterator<typename decltype(_order)::const_iterator>;

    using reverse_iterator = double_iterator<typename decltype(_order)::reverse_iterator>;
    using const_reverse_iterator =
      double_iterator<typename decltype(_order)::const_reverse_iterator>;

    ref_vec() = default;

    ref_vec(std::initializer_list<value_type*> lst) : _order {lst} { };

    template<typename InputIter, typename = std::enable_if_t<std::is_same_v<decltype(*std::declval<InputIter>()), value_type&>>>
    ref_vec(InputIter iter1, InputIter iter2) {
      _order.reserve(std::distance(iter1, iter2));
      std::transform(iter1, iter2, std::back_inserter(_order), [] (auto& v) {return &v; });      
    }

    template<typename Range, typename  = std::enable_if_t<std::is_same_v<decltype(*std::declval<Range>().begin()), value_type&>>>
    ref_vec(Range&& rng) : ref_vec (std::begin(rng), std::end(rng)) { }

    value_type& push_back(value_type& v)
    {
      _order.push_back(&v);
      return v;
    }

    value_type& push_back(non_null_ptr<value_type> ptr)
    {
      _order.push_back(ptr);
      return *ptr;
    }

    value_type& emplace_back(value_type& v)
    {
      return push_back(v);
    }

    std::unique_ptr<value_type> erase(const value_type& v)
    {
      auto iter =
        std::find_if(_order.begin(), _order.end(), [&v](auto&& ptr) { return ptr == &v; });
      if (iter != _order.end()) {
        auto uptr = std::move(*iter);
        _order.erase(iter);
        return uptr;
      }
      return nullptr;
    }

    iterator rotate_to_back(const value_type& v)
    {
      auto iter =
        std::find_if(_order.begin(), _order.end(), [&v](auto&& ptr) { return ptr == &v; });
      return rotate_to_back(iter);
    }

    iterator rotate_to_back(iterator iter)
    {
      if (iter != _order.end()) {
        {
          return std::rotate(iter.data(), iter.data() + 1, _order.end());
        }
      }
      return end();
    }

    iterator rotate_to_front(const value_type& v)
    {
      auto iter =
        std::find_if(_order.begin(), _order.end(), [&v](auto&& ptr) { return ptr == &v; });
      return rotate_to_front(iter);
    }

    iterator rotate_to_front(iterator iter)
    {
      if (iter != _order.end()) {
        {
          return std::rotate(_order.begin(), iter.data(), iter.data() + 1);
        }
      }
      return end();
    }

    std::size_t size() const noexcept
    {
      return _order.size();
    }

    bool empty() const noexcept
    {
      return _order.empty();
    }

    std::size_t capacity() const noexcept
    {
      return _order.capacity();
    }

    std::size_t max_size() const noexcept
    {
      return _order.max_size();
    }

    void reserve(std::size_t new_cap) 
    {
      _order.reserve(new_cap);
    }

    void shrink_to_fit()
    {
      _order.shrink_to_fit();
    }

    value_type& operator[](std::size_t n)
    {
      return *_order[n];
    }

    const value_type& operator[](std::size_t n) const
    {
      return *_order[n];
    }

    value_type& at(std::size_t n)
    {
      return *_order.at(n);
    }

    const value_type& at(std::size_t n) const
    {
      return *_order.at(n);
    }

    iterator begin()
    {
      return _order.begin();
    }
    iterator end()
    {
      return _order.end();
    }
    const_iterator begin() const
    {
      return _order.begin();
    }
    const_iterator end() const
    {
      return _order.end();
    }

    reverse_iterator rbegin()
    {
      return _order.rbegin();
    }
    reverse_iterator rend()
    {
      return _order.rend();
    }
    const_reverse_iterator rbegin() const
    {
      return _order.rbegin();
    }
    const_reverse_iterator rend() const
    {
      return _order.rend();
    }

    value_type& front()
    {
      return *_order.front();
    }

    value_type& back()
    {
      return *_order.back();
    }

    const value_type& front() const
    {
      return *_order.front();
    }

    const value_type& back() const
    {
      return *_order.back();
    }

    std::vector<value_type*>& underlying() {
      return _order;
    }
  };


} // namespace waybar::util
