#include <algorithm>
#include <chrono>
#include <format>
#include <memory>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "bench_utils.hpp"
#include "engine/order_book.hpp"
#include "models/basic_types.hpp"

using namespace stockex::models;
using namespace stockex::benchmarks;

const std::string IMPLEMENTATION = "bitmap_chunked_order_queue";
constexpr size_t TOTAL_EVENTS = 15'000'000;
constexpr size_t INITIAL_BOOK_DEPTH = 3'000'000;
constexpr int ORDER_TO_TRADE_RATIO = 50;
constexpr int ADD_PROBABILITY_PERCENT = 20;
constexpr Price BASE_PRICE = 5000;
constexpr double PRICE_STD_DEV = 10.0;

struct SimulationResults {
  std::vector<double> addLatencies;
  std::vector<double> cancelLatencies;
  std::vector<double> matchLatencies;
  std::size_t adds{};
  std::size_t cancels{};
  std::size_t matches{};
};

struct SimulationConfig {
  std::size_t totalEvents;
  int orderToTradeRatio;
  int addProbabilityPercent;
  Price basePrice;
  double priceStdDev;
};

SimulationResults runSimulation(stockex::engine::OrderBook &book,
                                std::mt19937 &rng,
                                std::vector<OrderId> &activeOrders,
                                OrderId &nextMarketOrderId,
                                const SimulationConfig &config) {

  SimulationResults results;

  results.addLatencies.reserve(config.totalEvents);
  results.cancelLatencies.reserve(config.totalEvents);
  results.matchLatencies.reserve(config.totalEvents / config.orderToTradeRatio);

  std::uniform_int_distribution actionDist(1, config.orderToTradeRatio);
  std::uniform_int_distribution addCancelDist(1, 100);
  std::uniform_int_distribution<Quantity> qty_dist(1, 100);
  std::normal_distribution<> priceDist(config.basePrice, config.priceStdDev);

  std::println("\n--- Starting simulation for {} events... ---", TOTAL_EVENTS);

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

int main(int argc, char **argv) {
  auto book = std::make_unique<stockex::engine::OrderBook>(1);
  std::mt19937 rng(42);

  std::uniform_int_distribution<Quantity> qty_dist(1, 100);
  std::normal_distribution<> priceDist(BASE_PRICE, PRICE_STD_DEV);

  std::vector<OrderId> activeOrders;
  activeOrders.reserve(INITIAL_BOOK_DEPTH +
                       (TOTAL_EVENTS * ADD_PROBABILITY_PERCENT / 100));
  OrderId nextMarketOrderId{};

  std::println("--- Pre-filling order book with {} orders... ---",
               INITIAL_BOOK_DEPTH);
  for (std::size_t i = 0; i < INITIAL_BOOK_DEPTH; ++i) {
    const auto price = static_cast<Price>(std::round(priceDist(rng)));
    const auto side = (price < BASE_PRICE) ? Side::BUY : Side::SELL;
    book->addOrder(1, nextMarketOrderId, nextMarketOrderId, side, price,
                   qty_dist(rng));
    activeOrders.push_back(nextMarketOrderId);
    nextMarketOrderId++;
  }
  std::println("Book pre-filled. Active orders: {}", activeOrders.size());

  if (argc == 2) {
    if (auto perfMode = parsePerfMode(argv[1]); perfMode != stockex::benchmarks::PerfMode::None) {
      runPerf(perfMode, "orderbook_benchmark");
    }
  }

  SimulationConfig config{.totalEvents = TOTAL_EVENTS,
                          .orderToTradeRatio = ORDER_TO_TRADE_RATIO,
                          .addProbabilityPercent = ADD_PROBABILITY_PERCENT,
                          .basePrice = BASE_PRICE,
                          .priceStdDev = PRICE_STD_DEV};

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
  stockex::benchmarks::saveLatenciesToFile(results.addLatencies,
                                           std::format("latencies_add_{}.txt", IMPLEMENTATION));
  stockex::benchmarks::saveLatenciesToFile(results.cancelLatencies,
                                           std::format("latencies_cancel_{}.txt", IMPLEMENTATION));
  stockex::benchmarks::saveLatenciesToFile(results.matchLatencies,
                                           std::format("latencies_match_{}.txt", IMPLEMENTATION));
  std::println("Data saved successfully.");

  return 0;
}