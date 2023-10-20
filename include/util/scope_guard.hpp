#pragma once

#include <utility>

namespace waybar::util {

template <typename Func>
class scope_guard {
 public:
  explicit scope_guard(Func&& exit_function) : f{std::forward<Func>(exit_function)} {}
  scope_guard(const scope_guard&) = delete;
  scope_guard(scope_guard&&) = default;
  scope_guard& operator=(const scope_guard&) = delete;
  scope_guard& operator=(scope_guard&&) = default;
  ~scope_guard() { f(); }

 private:
  Func f;
};

}  // namespace waybar::util
