#include <chrono>
#include <cstddef>
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

constexpr Price TEST_PRICE = 5000;

int main(int argc, char **argv) {
  if (argc != 5) {
    std::print(stderr,
               "Usage: {} <implementation_name> <active_orders> "
               "<fragmentation_ratio> <match_quantity>\n",
               argv[0]);
    exit(1);
  }

  std::string implName = argv[1];
  std::size_t activeOrdersToMatch;
  std::size_t fragmentationRatio;
  Quantity matchQty;

  try {
    activeOrdersToMatch = std::stoul(argv[2]);
    fragmentationRatio = std::stoul(argv[3]);
    matchQty = static_cast<Quantity>(std::stoul(argv[4]));
  } catch (const std::exception &e) {
    std::print(stderr, "Error: Invalid numeric argument. {}\n", e.what());
    exit(1);
  }

  const auto totalOrdersToAdd = activeOrdersToMatch * fragmentationRatio;
  if (totalOrdersToAdd > stockex::models::MAX_NUM_ORDERS) {
    std::print(stderr, "Error: Test configuration exceeds system limits.\n");
    std::print(stderr, "  Total orders required ({} * {}) = {}\n",
               activeOrdersToMatch, fragmentationRatio, totalOrdersToAdd);
    std::print(stderr, "  Maximum allowed orders = {}\n",
               stockex::models::MAX_NUM_ORDERS);
    exit(1);
  }

  std::println("--- Starting Queue Fragmentation Test for: {} ---", implName);
  std::println("--- Creating 1 active order for every {} deleted orders ---",
               fragmentationRatio > 1 ? fragmentationRatio - 1 : 0);

  auto book = std::make_unique<stockex::engine::OrderBook>(1);
  std::vector<double> latencies;
  latencies.reserve(activeOrdersToMatch);
  std::vector<OrderId> allOrderIds;
  allOrderIds.reserve(totalOrdersToAdd);

  for (OrderId i = 0; i < totalOrdersToAdd; ++i) {
    book->addOrder(1, i, i, Side::BUY, TEST_PRICE, 1);
    allOrderIds.push_back(i);
  }

  for (size_t i = 0; i < totalOrdersToAdd; ++i) {
    if ((i + 1) % fragmentationRatio != 0) {
      book->removeOrder(1, allOrderIds[i]);
    }
  }

  std::size_t ordersMatchedSoFar{};
  std::size_t matchAttempts{};

  while (ordersMatchedSoFar < activeOrdersToMatch) {
    matchAttempts++;
    auto start = std::chrono::high_resolution_clock::now();
    auto result = book->match(2, 99999999, Side::SELL, TEST_PRICE, matchQty);
    auto end = std::chrono::high_resolution_clock::now();

    if (!result.matches_.empty()) {
      latencies.push_back(
          std::chrono::duration<double, std::micro>(end - start).count());
      ordersMatchedSoFar += result.matches_.size();
    } else {
      std::print(stderr,
                 "Error: Expected a match but found none on attempt #{}.\n",
                 matchAttempts);
      std::print(stderr, "  Matched {} out of {} total active orders.\n",
                 ordersMatchedSoFar, activeOrdersToMatch);
      break;
    }
  }

  std::println("\n--- Fragmentation Test Complete ---");
  stockex::benchmarks::printMetrics(latencies, latencies.size());

  auto filename = std::format("latencies_fragmentation_test_{}.txt", implName);
  stockex::benchmarks::saveLatenciesToFile(latencies, filename);
  std::println("Data saved successfully to {}", filename);

  return 0;
}