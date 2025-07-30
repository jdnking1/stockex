#include <chrono>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <vector>

#include "bench_utils.hpp"
#include "engine/order_book.hpp"
#include "models/basic_types.hpp"

using namespace stockex::models;
using namespace stockex::benchmarks;

constexpr int NUM_ITERATIONS = 1000;
constexpr size_t ORDERS_PER_ITERATION = 10000;
constexpr size_t ORDERS_TO_FILL_PER_SWEEP = 1000;
constexpr Price TEST_PRICE = 5000;

int main(int argc, char **argv) {
  if (argc != 2) {
    std::print(stderr, "Usage: {} <implementation_name>\n", argv[0]);
    exit(1);
  }
  std::string implName = argv[1];

  std::println("--- Starting Sweep Test for: {} ---", implName);
  std::println("--- Fills per sweep: {} ---", ORDERS_TO_FILL_PER_SWEEP);

  auto book = std::make_unique<stockex::engine::OrderBook>(1);

  std::vector<double> latencies;
  latencies.reserve(NUM_ITERATIONS);

  for (int i = 0; i < NUM_ITERATIONS; ++i) {
    for (size_t j = 0; j < ORDERS_PER_ITERATION; ++j) {
      auto id = static_cast<OrderId>((i * ORDERS_PER_ITERATION) + j);
      book->addOrder(1, id, id, Side::BUY, TEST_PRICE, 1);
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto result = book->match(2, 99999999, Side::SELL, TEST_PRICE,
                              ORDERS_TO_FILL_PER_SWEEP);
    auto end = std::chrono::high_resolution_clock::now();

    if (result.matches_.size() == ORDERS_TO_FILL_PER_SWEEP) {
      latencies.push_back(
          std::chrono::duration<double, std::micro>(end - start).count());
    } else {
      std::print(stderr,
                 "Error: Did not match the expected number of orders. Expected "
                 "{}, got {}.\n",
                 ORDERS_TO_FILL_PER_SWEEP, result.matches_.size());
    }

    auto r [[maybe_unused]] = book->match(2, 99999998, Side::SELL, TEST_PRICE,
                                          ORDERS_PER_ITERATION * 2);
  }

  std::println("\n--- Sweep Test Complete ---");
  stockex::benchmarks::printMetrics(latencies, latencies.size());

  auto filename = std::format("latencies_sweep_test_{}.txt", implName);
  stockex::benchmarks::saveLatenciesToFile(latencies, filename);
  std::println("Data saved successfully to {}", filename);

  return 0;
}