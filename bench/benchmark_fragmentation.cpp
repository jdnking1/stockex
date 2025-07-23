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

constexpr size_t NUM_VALID_ORDERS = 1000;
constexpr int VALID_ORDER_INTERVAL = 10000;
constexpr Price TEST_PRICE = 5000;

int main(int argc, char **argv) {
  if (argc != 2) {
    std::print(stderr, "Usage: {} <implementation_name>\n", argv[0]);
    exit(1);
  }
  std::string impl_name = argv[1];
  std::println(
      "--- Starting FINAL CORRECTED Interspersed Torture Test for: {} ---",
      impl_name);
  std::println("--- Creating a single large queue with 1 valid order every {} "
               "deleted orders ---",
               VALID_ORDER_INTERVAL - 1);

  auto book = std::make_unique<stockex::engine::OrderBook>(1);

  std::vector<double> latencies;
  constexpr size_t total_orders_to_add =
      NUM_VALID_ORDERS * VALID_ORDER_INTERVAL;
  latencies.reserve(NUM_VALID_ORDERS);

  std::vector<OrderId> all_order_ids;
  all_order_ids.reserve(total_orders_to_add);

  for (size_t i = 0; i < total_orders_to_add; ++i) {
    book->addOrder(1, i, i, Side::BUY, TEST_PRICE, 1);
    all_order_ids.push_back(i);
  }

  for (size_t i = 0; i < total_orders_to_add; ++i) {
    if ((i + 1) % VALID_ORDER_INTERVAL != 0) {
      book->removeOrder(1, all_order_ids[i]);
    }
  }

  for (size_t k = 0; k < NUM_VALID_ORDERS; ++k) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = book->match(2, 99999999, Side::SELL, TEST_PRICE, 20);
    auto end = std::chrono::high_resolution_clock::now();

    if (!result.matches_.empty()) {
      latencies.push_back(
          std::chrono::duration<double, std::micro>(end - start).count());
    } else {
      std::print(stderr,
                 "Error: Expected a match but found none on valid order #{}.\n",
                 k + 1);
      break;
    }
  }

  std::println("\n--- Torture Test Complete ---");
  stockex::benchmarks::printMetrics(latencies, latencies.size());

  std::string filename =
      std::format("latencies_final_torture_test_{}.txt", impl_name);
  stockex::benchmarks::saveLatenciesToFile(latencies, filename);
  std::println("Data saved successfully to {}", filename);

  return 0;
}
