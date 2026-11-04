#pragma once

#include <cstddef>
#include <iterator>
#include <regex>
#include <string>

namespace waybar::util {

/* Text from another program (a window title, an app id) to match against a config regex.
 * libstdc++'s std::regex backtracks without limit, so a rule with N wildcards can take length^N
 * steps on crafted text. The iterators count the characters the matcher reads and throw
 * regex_error(error_complexity) past MAX_STEPS, as libc++ does natively.
 */
class RegexInput {
 public:
  static constexpr std::size_t MAX_STEPS = 250'000;

  class Iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = char;
    using difference_type = std::ptrdiff_t;
    using pointer = const char*;
    using reference = const char&;

    Iterator() = default;
    Iterator(pointer pos, std::size_t* steps) : pos_(pos), steps_(steps) {}

    reference operator*() const {
      if (++*steps_ > MAX_STEPS) {
        throw std::regex_error(std::regex_constants::error_complexity);
      }
      return *pos_;
    }
    Iterator& operator++() {
      ++pos_;
      return *this;
    }
    Iterator operator++(int) { return {pos_++, steps_}; }
    Iterator& operator--() {
      --pos_;
      return *this;
    }
    Iterator operator--(int) { return {pos_--, steps_}; }
    bool operator==(const Iterator& other) const { return pos_ == other.pos_; }

   private:
    pointer pos_ = nullptr;
    std::size_t* steps_ = nullptr;
  };

  using Match = std::match_results<Iterator>;

  explicit RegexInput(const std::string& text) : text_(text) {}

  Iterator begin() { return {text_.data(), &steps_}; }
  Iterator end() { return {text_.data() + text_.size(), &steps_}; }

 private:
  const std::string& text_;
  std::size_t steps_ = 0;
};

}  // namespace waybar::util
