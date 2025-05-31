#include <print>

#include <utils/memory_pool.hpp>

int main() {
  stockex::utils::MemoryPool<double> x{22};

  auto ptr = x.alloc(22);

  std::print("w {}  {}\n", 23, *ptr);

  x.free(ptr);
}