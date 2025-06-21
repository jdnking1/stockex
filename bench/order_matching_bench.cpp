#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <memory>
#include <numeric>
#include <string>
#include <unistd.h>

#include "bench_utils.hpp"
#include "engine/order_book.hpp"
#include "models/basic_types.hpp"
#include "models/constants.hpp"

using namespace stockex::benchmarks;

using namespace stockex::models;

enum class TestType { Flat, Nonlinear, Fanout };

TestType parseTestType(const std::string &arg) {
  using enum TestType;
  if (arg == "flat")
    return Flat;
  if (arg == "nonlinear")
    return Nonlinear;
  if (arg == "fanout")
    return Fanout;
  std::print("Unknown test type: {}\n", arg);
  exit(1);
}

void printMetrics(std::vector<double> &latencies, size_t totalMatches) {
  std::ranges::sort(latencies);
  auto size = static_cast<double>(latencies.size());
  auto sum = std::accumulate(latencies.begin(), latencies.end(), 0.0); // in us
  auto minLat = latencies.front();
  auto maxLat = latencies.back();
  auto avg = sum / size;
  auto p99index = static_cast<int>(size * 0.99);
  auto p99 = latencies[p99index];
  auto var = std::accumulate(
      latencies.begin(), latencies.end(), 0.0,
      [avg](double acc, double v) { return acc + (v - avg) * (v - avg); });
  auto stddev = std::sqrt(var / size);
  auto throughput =
      static_cast<double>(totalMatches) / (sum / 1'000'000.0); // matches/sec

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
    std::cerr
        << "Usage: " << argv[0]
        << " [flat|nonlinear|fanout|skewed] [--perf=record|--perf=stat]\n";
    return 1;
  }
  using enum stockex::benchmarks::PerfMode;
  using enum Side;

  const std::string testName = argv[1];
  auto book = std::make_unique<stockex::engine::OrderBook>(1);

  const auto type = parseTestType(testName);
  const auto basePrice = 100;
  const auto numOrders = 7'500'000;
  const auto matchQty = (type == TestType::Fanout) ? 10'000 : 1000;
  const auto orderQty = (type == TestType::Fanout) ? 10 : 50;

  for (OrderId i = 0; i < numOrders; ++i) {
    int price = 100;
    switch (type) {
      using enum TestType;
    case Flat:
      price = basePrice + (i % MAX_PRICE_LEVELS);
      break;
    case Nonlinear:
      price =
          basePrice + static_cast<int>(pow(i % 100, 1.5)) % MAX_PRICE_LEVELS;
      break;
    case Fanout:
      price = basePrice + (i % 10);
      break;
    }
    book->addOrder(1, i, i, BUY, price, orderQty);
  }

  std::vector<double> latencies;
  latencies.reserve(numOrders);

  if (auto perfMode = parsePerfMode(argc, argv); perfMode != None) {
    runPerf(perfMode, testName);
  }

  size_t totalMatches = 0;

  for (int i = 0; i < numOrders; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result =
        book->match(2, 1, SELL, basePrice + (i % MAX_PRICE_LEVELS), matchQty);
    auto end = std::chrono::high_resolution_clock::now();
    if (!result.matches_.empty()) {
      std::chrono::duration<double, std::micro> duration = end - start;
      totalMatches += result.matches_.size();
      latencies.push_back(duration.count());
    }
  }
  printMetrics(latencies, totalMatches);
  return 0;
}
