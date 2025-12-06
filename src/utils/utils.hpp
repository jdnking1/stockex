#pragma once

#include <cstdlib>
#include <iostream>
#include <pthread.h>
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

inline auto pinToCore(int coreId) -> bool {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(coreId, &cpuset);
  return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
}

} // namespace stockex::utils