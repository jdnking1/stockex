#pragma once

#include <cstdlib>
#include <iostream>
#include <print>
#include <string_view>

namespace stockex::utils {

inline auto ASSERT(bool condition, std::string_view message) noexcept {
  if (!condition) [[unlikely]] {
    [&]() __attribute__((noinline, cold)) {
      std::cerr << message << std::endl;
      std::exit(EXIT_FAILURE);
    }();
  }
}

} // namespace stockex::utils