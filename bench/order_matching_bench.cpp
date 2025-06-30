#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <memory>
#include <numeric>
#include <print>
#include <random>
#include <string>
#include <unistd.h>

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

int generatePrice(TestType type, unsigned long i, int basePrice,
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
    int level = (i % 5) * 5;
    return basePrice + level;
  }
  case RandomWalk: {
    static int lastPrice = basePrice;
    int delta = (rng() % 3) - 1;
    lastPrice = std::clamp(lastPrice + delta, 0,
                           static_cast<int>(MAX_PRICE_LEVELS - 1));
    return lastPrice;
  }
  }
  return basePrice;
}

void populateBook(stockex::engine::OrderBook &book, TestType type,
                  int basePrice, size_t numOrders, int orderQty) {
  using enum Side;
  using enum TestType;
  std::mt19937 rng(42);
  for (OrderId i = 0; i < numOrders; ++i) {
    int price = generatePrice(type, i, basePrice, rng);
    book.addOrder(1, i, i, BUY, price, orderQty);
  }
}


void printMetrics(std::vector<double> &latencies, size_t totalMatches) {
  std::ranges::sort(latencies);
  auto size = static_cast<double>(latencies.size());
  auto sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
  auto minLat = latencies.front();
  auto maxLat = latencies.back();
  auto avg = sum / size;
  auto p99index = static_cast<int>(size * 0.99);
  auto p99 = latencies[p99index];
  auto var = std::accumulate(
      latencies.begin(), latencies.end(), 0.0,
      [avg](double acc, double v) { return acc + (v - avg) * (v - avg); });
  auto stddev = std::sqrt(var / size);
  auto throughput = static_cast<double>(totalMatches) / (sum / 1'000'000.0);

  std::print("Total time: {} us\n", sum);
  std::print("Total matches: {}\n", totalMatches);
  std::print("Average latency: {} us\n", avg);
  std::print("99th percentile latency: {} us\n", p99);
  std::print("Min latency: {} us\n", minLat);
  std::print("Max latency: {} us\n", maxLat);
  std::print("Standard deviation: {} us\n", stddev);
  std::print("Throughput: {} matches/sec\n", throughput);
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0]
              << " [flat|nonlinear|fanout|skewed|layered|randomwalk|crossed|"
                 "burst|thinheavy] [--perf=record|--perf=stat]\n";
    return 1;
  }

  using enum stockex::benchmarks::PerfMode;
  using enum Side;

  const std::string testName = argv[1];
  auto book = std::make_unique<stockex::engine::OrderBook>(1);

  const auto type = parseTestType(testName);
  const auto basePrice = 100;
  const auto numOrders = 5'000'000;
  const auto orderQty = (type == TestType::Fanout) ? 10 : 50;
  const auto matchQty = (type == TestType::Fanout) ? 10'000 : 1'000;

  std::print("--- Book pre-fill (untimed) ---\n");
  populateBook(*book, type, basePrice, numOrders, orderQty);

  std::vector<double> latencies;
  latencies.reserve(numOrders);

  if (auto perfMode = parsePerfMode(argc, argv); perfMode != None) {
    runPerf(perfMode, testName);
  }

  std::print("--- Benchmarking match() ---\n");
  size_t totalMatches = 0;
  std::mt19937 rng(42);

  for (int i = 0; i < numOrders; ++i) {
    int price = generatePrice(type, i, basePrice, rng);
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
  return 0;
}
