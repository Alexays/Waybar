#pragma once

#include <utility>

namespace waybar::util {

template <typename Func>
class ScopeGuard {
 public:
  explicit ScopeGuard(Func&& exit_function) : f{std::forward<Func>(exit_function)} {}
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard(ScopeGuard&&) = default;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
  ScopeGuard& operator=(ScopeGuard&&) = default;
  ~ScopeGuard() { f(); }

 private:
  Func f;
};

}  // namespace waybar::util
