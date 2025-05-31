#include <print>

#include <utils/memory_pool.hpp>

int main() {
  stockex::utils::MemoryPool<double> x{22};

  auto ptr = x.alloc(22);

  x.free(ptr);

  std::print("w {}  {}\n", 23, *ptr);
}