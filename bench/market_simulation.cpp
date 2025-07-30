#include <algorithm>
#include <chrono>
#include <format>
#include <memory>
#include <print>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "bench_utils.hpp"
#include "engine/order_book.hpp"
#include "models/basic_types.hpp"

using namespace stockex::models;
using namespace stockex::benchmarks;

constexpr std::size_t TOTAL_EVENTS = 5'000'000;
constexpr Price BASE_PRICE = 5000;

struct SimulationConfig {
  std::string implementationName;
  std::string scenarioName;
  std::size_t totalEvents;
  std::size_t initialBookDepth;
  int orderToTradeRatio;
  int addProbabilityPercent;
  Price basePrice;
  double priceStdDev;
};

struct SimulationResults {
  std::vector<double> addLatencies;
  std::vector<double> cancelLatencies;
  std::vector<double> matchLatencies;
  std::size_t adds{};
  std::size_t cancels{};
  std::size_t matches{};
};

auto runSimulation(stockex::engine::OrderBook &book, std::mt19937 &rng,
                   std::vector<OrderId> &activeOrders,
                   OrderId &nextMarketOrderId, const SimulationConfig &config)
    -> SimulationResults {

  SimulationResults results;
  results.addLatencies.reserve(config.totalEvents);
  results.cancelLatencies.reserve(config.totalEvents);
  results.matchLatencies.reserve(config.totalEvents / config.orderToTradeRatio);

  std::uniform_int_distribution actionDist(1, config.orderToTradeRatio);
  std::uniform_int_distribution addCancelDist(1, 100);
  std::uniform_int_distribution<Quantity> qty_dist(1, 100);
  std::normal_distribution<> priceDist(config.basePrice, config.priceStdDev);

  std::println("\n--- Starting simulation for {} events... ---", TOTAL_EVENTS);
  std::println("--- Scenario: {}, Price StdDev: {}, Initial Depth: {} ---",
               config.scenarioName, config.priceStdDev,
               config.initialBookDepth);

  for (std::size_t i = 0; i < config.totalEvents; ++i) {
    const int eventType = actionDist(rng);

    if (eventType < config.orderToTradeRatio) {
      if (addCancelDist(rng) <= config.addProbabilityPercent) {
        const auto price = static_cast<Price>(std::round(priceDist(rng)));
        const Side side = (price < config.basePrice) ? Side::BUY : Side::SELL;

        auto start = std::chrono::high_resolution_clock::now();
        book.addOrder(1, nextMarketOrderId, nextMarketOrderId, side, price,
                      qty_dist(rng));
        auto end = std::chrono::high_resolution_clock::now();

        results.addLatencies.push_back(
            std::chrono::duration<double, std::micro>(end - start).count());
        activeOrders.push_back(nextMarketOrderId);
        nextMarketOrderId++;
        results.adds++;
      } else if (!activeOrders.empty()) {
        std::uniform_int_distribution<std::size_t> cancelIndexDist(
            0, activeOrders.size() - 1);
        auto idxToCancel = cancelIndexDist(rng);
        auto orderToCancel = activeOrders[idxToCancel];

        auto start = std::chrono::high_resolution_clock::now();
        book.removeOrder(1, orderToCancel);
        auto end = std::chrono::high_resolution_clock::now();

        results.cancelLatencies.push_back(
            std::chrono::duration<double, std::micro>(end - start).count());
        activeOrders[idxToCancel] = activeOrders.back();
        activeOrders.pop_back();
        results.cancels++;
      }
    } else {
      const auto side = (i % 2 == 0) ? Side::SELL : Side::BUY;
      const Price price = (side == Side::SELL) ? (config.basePrice - 20)
                                               : (config.basePrice + 20);
      const Quantity quantity = qty_dist(rng) * 5;

      auto start = std::chrono::high_resolution_clock::now();
      auto matchResult =
          book.match(2, nextMarketOrderId, side, price, quantity);
      auto end = std::chrono::high_resolution_clock::now();

      results.matchLatencies.push_back(
          std::chrono::duration<double, std::micro>(end - start).count());
      nextMarketOrderId++;
      results.matches++;

      if (!matchResult.matches_.empty()) {
        std::ranges::for_each(matchResult.matches_,
                              [&activeOrders](const auto &match) {
                                std::erase(activeOrders, match.matchedOrderId_);
                              });
      }
    }
  }
  return results;
}

auto parseConfig(int argc, char **argv) -> SimulationConfig {
  if (argc != 4) {
    std::print(stderr,
               "Usage: {} <implementation_name> <scenario> <price_std_dev>\n",
               argv[0]);
    std::print(stderr, "Scenarios: add_heavy, cancel_heavy, match_heavy\n");
    exit(1);
  }

  SimulationConfig config;
  config.implementationName = argv[1];
  config.scenarioName = argv[2];
  try {
    config.priceStdDev = std::stod(argv[3]);
  } catch (const std::invalid_argument &) {
    std::print(stderr, "Error: Invalid numeric argument for price_std_dev.\n");
    exit(1);
  }

  config.totalEvents = TOTAL_EVENTS;
  config.basePrice = BASE_PRICE;

  if (auto scenario = config.scenarioName; scenario == "add_heavy") {
    config.orderToTradeRatio = 50;
    config.addProbabilityPercent = 80; // 80% adds, 20% cancels
    config.initialBookDepth = 100'000;
  } else if (scenario == "cancel_heavy") {
    config.orderToTradeRatio = 50;
    config.addProbabilityPercent = 20; // 20% adds, 80% cancels
    config.initialBookDepth = 1'000'000;
  } else if (scenario == "match_heavy") {
    config.orderToTradeRatio = 5; // 1 match for every 4 add/cancels
    config.addProbabilityPercent = 55;
    config.initialBookDepth = 1'000'000;
  } else {
    std::print(stderr, "Unknown scenario: {}\n", scenario);
    exit(1);
  }

  return config;
}

int main(int argc, char **argv) {
  SimulationConfig config = parseConfig(argc, argv);

  auto book = std::make_unique<stockex::engine::OrderBook>(1);
  std::mt19937 rng(42);

  std::uniform_int_distribution<Quantity> qty_dist(1, 100);
  std::normal_distribution<> priceDist(config.basePrice, config.priceStdDev);

  std::vector<OrderId> activeOrders;
  activeOrders.reserve(config.initialBookDepth +
                       (TOTAL_EVENTS * config.addProbabilityPercent / 100));
  OrderId nextMarketOrderId{};

  std::println("--- Pre-filling order book with {} orders... ---",
               config.initialBookDepth);
  for (std::size_t i = 0; i < config.initialBookDepth; ++i) {
    const auto price = static_cast<Price>(std::round(priceDist(rng)));
    const auto side = (price < config.basePrice) ? Side::BUY : Side::SELL;
    book->addOrder(1, nextMarketOrderId, nextMarketOrderId, side, price,
                   qty_dist(rng));
    activeOrders.push_back(nextMarketOrderId);
    nextMarketOrderId++;
  }
  std::println("Book pre-filled. Active orders: {}", activeOrders.size());

  auto start = std::chrono::high_resolution_clock::now();
  auto results =
      runSimulation(*book, rng, activeOrders, nextMarketOrderId, config);
  auto end = std::chrono::high_resolution_clock::now();

  auto simulationTime = std::chrono::duration<double>(end - start).count();

  std::println("\n--- Simulation Complete---");
  std::println("Time Elapsed: {}s", simulationTime);
  std::println("Adds: {}, Cancels: {}, Matches: {}", results.adds,
               results.cancels, results.matches);

  std::println("\n--- Add Order Metrics ---");
  stockex::benchmarks::printMetrics(results.addLatencies, results.adds);

  std::println("\n--- Cancel Order Metrics ---");
  stockex::benchmarks::printMetrics(results.cancelLatencies, results.cancels);

  std::println("\n--- Match Operation Metrics ---");
  stockex::benchmarks::printMetrics(results.matchLatencies, results.matches);

  std::println("\n--- Saving latency data to files... ---");
  std::string suffix =
      std::format("{}_{}_{}", config.implementationName, config.scenarioName,
                  static_cast<int>(config.priceStdDev));
  stockex::benchmarks::saveLatenciesToFile(
      results.addLatencies, std::format("latencies_add_{}.txt", suffix));
  stockex::benchmarks::saveLatenciesToFile(
      results.cancelLatencies, std::format("latencies_cancel_{}.txt", suffix));
  stockex::benchmarks::saveLatenciesToFile(
      results.matchLatencies, std::format("latencies_match_{}.txt", suffix));
  std::println("Data saved successfully.");

  return 0;
}
