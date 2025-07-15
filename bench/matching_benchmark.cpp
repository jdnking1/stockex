#include <algorithm>
#include <chrono>
#include <ctime>
#include <memory>
#include <print>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

#include "bench_utils.hpp"
#include "engine/order_book.hpp"
#include "models/basic_types.hpp"
#include "models/constants.hpp"

using namespace stockex::benchmarks;
using namespace stockex::models;

enum class TestType {
  Flat,
  Nonlinear,
  Fanout,
  Skewed,
  Layered,
  RandomWalk,
};

TestType parseTestType(const std::string &arg) {
  using enum TestType;
  if (arg == "flat")
    return Flat;
  if (arg == "nonlinear")
    return Nonlinear;
  if (arg == "fanout")
    return Fanout;
  if (arg == "skewed")
    return Skewed;
  if (arg == "layered")
    return Layered;
  if (arg == "randomwalk")
    return RandomWalk;
  std::print("Unknown test type: {}\n", arg);
  exit(1);
}

// Generates a price based on the test type
Price generatePrice(TestType type, OrderId i, Price basePrice,
                    std::mt19937 &rng) {
  using enum TestType;
  switch (type) {
  case Flat:
    return basePrice + (i % MAX_PRICE_LEVELS);
  case Nonlinear: {
    int x = i % 100;
    return basePrice + ((x * (x + 5)) / 10) % MAX_PRICE_LEVELS;
  }
  case Fanout:
    return basePrice + (i % 10);
  case Skewed:
    return basePrice + (i % 20);
  case Layered: {
    Price level = (i % 5) * 5;
    return basePrice + level;
  }
  case RandomWalk: {
    static Price lastPrice = basePrice;
    int delta = (rng() % 3) - 1;
    auto result = std::clamp(lastPrice + delta, 0,
                             static_cast<int>(MAX_PRICE_LEVELS - 1));
    lastPrice = static_cast<Price>(result);
    return lastPrice;
  }
  }
  return basePrice;
}

void populateBook(stockex::engine::OrderBook &book, TestType type,
                  Price basePrice, size_t numOrders, Quantity orderQty) {
  using enum Side;
  using enum TestType;
  std::mt19937 rng(42);
  for (OrderId i = 0; i < numOrders; ++i) {
    auto price = generatePrice(type, i, basePrice, rng);
    book.addOrder(1, i, i, BUY, price, orderQty);
  }
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0]
              << " [flat|nonlinear|fanout|skewed|layered|randomwalk|]"
                 "[--perf = record | --perf = stat]\n ";
    return 1;
  }

  using enum stockex::benchmarks::PerfMode;
  using enum Side;

  const std::string testName = argv[1];
  auto book = std::make_unique<stockex::engine::OrderBook>(1);

  const auto type = parseTestType(testName);
  const auto numOrders = static_cast<int>(MAX_NUM_ORDERS);
  const Price basePrice = 100;
  const Quantity orderQty = (type == TestType::Fanout) ? 10 : 50;
  const Quantity matchQty = (type == TestType::Fanout) ? 10'000 : 1'000;

  std::print("--- Book pre-fill (untimed) ---\n");
  populateBook(*book, type, basePrice, numOrders, orderQty);
  std::print("reserving\n");
  std::vector<double> latencies;
  latencies.reserve(1250300);

  if (auto perfMode = parsePerfMode(argc, argv); perfMode != None) {
    runPerf(perfMode, testName);
  }

  std::print("--- Benchmarking match() ---\n");
  size_t totalMatches = 0;
  std::mt19937 rng(42);

  for (int i = 0; i < numOrders; ++i) {
    auto price = generatePrice(type, i, basePrice, rng);
    auto start = std::chrono::high_resolution_clock::now();
    auto result = book->match(2, 1, SELL, price, matchQty);
    auto end = std::chrono::high_resolution_clock::now();
    if (!result.matches_.empty()) {
      std::chrono::duration<double, std::micro> duration = end - start;
      latencies.push_back(duration.count());
      totalMatches += result.matches_.size();
    }
  }

  printMetrics(latencies, totalMatches);

  std::string filename = "latencies_chunkedqueue_" + testName + ".txt";
  saveLatenciesToFile(latencies, filename);

  return 0;
}
