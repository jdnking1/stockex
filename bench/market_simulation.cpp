#include <algorithm>
#include <chrono>
#include <format>
#include <memory>
#include <print>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "bench_utils.hpp"
#include "engine/order_book.hpp"
#include "models/basic_types.hpp"

using namespace stockex::models;
using namespace stockex::benchmarks;

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

enum class OperationType { ADD, CANCEL, MATCH };

struct LogEntry {
  OperationType type;
  double latency;
  OrderId orderId;
  Price price;
  Quantity quantity;
  Side side;
};

struct SimulationResults {
  std::size_t adds = 0;
  std::size_t cancels = 0;
  std::size_t matches = 0;
  std::vector<LogEntry> eventLogs;
};

struct ActiveOrderDetails {
  ClientId clientId;
  Price price;
  Quantity quantity;
  Side side;
};

auto saveLogsToFile(const SimulationResults &results, std::ofstream &outputFile)
    -> void {
  if (!outputFile.is_open()) {
    std::print(stderr, "Error: Log file is not open for writing.\n");
    return;
  }

  outputFile << std::fixed << std::setprecision(2);

  for (const auto &log : results.eventLogs) {
    switch (log.type) {
      using enum OperationType;
    case ADD:
      outputFile << "ADD order " << log.orderId << " (price: " << log.price
                 << ", qty: " << log.quantity
                 << ", side: " << sideToString(log.side) << ") -> "
                 << log.latency << " us\n";
      break;
    case CANCEL:
      outputFile << "CANCEL order " << log.orderId << " (price: " << log.price
                 << ", qty: " << log.quantity
                 << ", side: " << sideToString(log.side) << ") -> "
                 << log.latency << " us\n";
      break;
    case MATCH:
      outputFile << "MATCH with order " << log.orderId
                 << " (price: " << log.price << ", qty: " << log.quantity
                 << ", side: " << sideToString(log.side) << ") -> "
                 << log.latency << " us\n";
      break;
    }
  }

  std::print("Successfully appended {} timed log entries to the file.\n",
             results.eventLogs.size());
}

auto prefillOrderBook(
    stockex::engine::OrderBook &book, std::mt19937 &rng,
    std::unordered_map<OrderId, ActiveOrderDetails> &activeOrdersMap,
    std::vector<OrderId> &activeOrdersVec, OrderId &nextMarketOrderId,
    const SimulationConfig &config, std::ofstream &logFile) -> void {

  logFile << "-- Start Prefilling --\n";

  std::normal_distribution<> priceDist(config.basePrice, config.priceStdDev);
  std::uniform_int_distribution<Quantity> qtyDist(1, 100);

  for (std::size_t i = 0; i < config.initialBookDepth; ++i) {
    const auto price = static_cast<Price>(std::round(priceDist(rng)));
    const auto quantity = qtyDist(rng);
    const auto side = (price < config.basePrice) ? Side::BUY : Side::SELL;
    const ClientId clientId = 1;
    const OrderId orderId = nextMarketOrderId;

    book.addOrder(clientId, orderId, orderId, side, price, quantity);

    activeOrdersMap[orderId] = {clientId, price, quantity, side};
    activeOrdersVec.push_back(orderId);
    nextMarketOrderId++;

    logFile << "PREFILL order " << orderId << " (price: " << price
            << ", qty: " << quantity << ", side: " << sideToString(side)
            << ")\n";
  }
  logFile << "-- End Prefilling --\n";

  std::println(
      "Successfully pre-filled order book with {} orders and logged them.\n",
      config.initialBookDepth);
}

auto handleAddOperation(
    stockex::engine::OrderBook &book,
    std::unordered_map<OrderId, ActiveOrderDetails> &activeOrdersMap,
    std::vector<OrderId> &activeOrdersVec, OrderId &nextMarketOrderId,
    const SimulationConfig &config, SimulationResults &results,
    std::normal_distribution<> &priceDist,
    std::uniform_int_distribution<Quantity> &qtyDist, std::mt19937 &rng)
    -> void {
  const auto price = static_cast<Price>(std::round(priceDist(rng)));
  const auto quantity = qtyDist(rng);
  const Side side = (price < config.basePrice) ? Side::BUY : Side::SELL;
  const ClientId clientId = 1;
  const OrderId newOrderId = nextMarketOrderId++;

  auto start = std::chrono::high_resolution_clock::now();
  book.addOrder(clientId, newOrderId, newOrderId, side, price, quantity);
  auto end = std::chrono::high_resolution_clock::now();

  auto latency = std::chrono::duration<double, std::micro>(end - start).count();
  results.eventLogs.emplace_back(OperationType::ADD, latency, newOrderId, price,
                                 quantity, side);

  activeOrdersMap[newOrderId] = {clientId, price, quantity, side};
  activeOrdersVec.push_back(newOrderId);
  results.adds++;
}

auto handleCancelOperation(
    stockex::engine::OrderBook &book,
    std::unordered_map<OrderId, ActiveOrderDetails> &activeOrdersMap,
    std::vector<OrderId> &activeOrdersVec, SimulationResults &results,
    std::mt19937 &rng) -> void {
  if (activeOrdersVec.empty()) {
    return;
  }

  std::uniform_int_distribution<std::size_t> cancelIndexDist(
      0, activeOrdersVec.size() - 1);
  auto vecIdx = cancelIndexDist(rng);
  OrderId orderToCancel = activeOrdersVec[vecIdx];

  activeOrdersVec[vecIdx] = activeOrdersVec.back();
  activeOrdersVec.pop_back();

  auto it = activeOrdersMap.find(orderToCancel);
  if (it != activeOrdersMap.end()) {
    const auto orderDetails = it->second;
    activeOrdersMap.erase(it);

    auto start = std::chrono::high_resolution_clock::now();
    book.removeOrder(orderDetails.clientId, orderToCancel);
    auto end = std::chrono::high_resolution_clock::now();

    auto latency =
        std::chrono::duration<double, std::micro>(end - start).count();
    results.eventLogs.emplace_back(OperationType::CANCEL, latency,
                                   orderToCancel, orderDetails.price,
                                   orderDetails.quantity, orderDetails.side);
    results.cancels++;
  }
}

auto handleMatchOperation(
    stockex::engine::OrderBook &book,
    std::unordered_map<OrderId, ActiveOrderDetails> &activeOrdersMap,
    OrderId &nextMarketOrderId, const SimulationConfig &config,
    SimulationResults &results,
    std::uniform_int_distribution<Quantity> &qtyDist, Side side,
    std::mt19937 &rng) -> void {
  const Price price =
      (side == Side::SELL) ? (config.basePrice - 20) : (config.basePrice + 20);
  const Quantity quantity = qtyDist(rng) * 5;
  const OrderId matchOrderId = nextMarketOrderId++;

  auto start = std::chrono::high_resolution_clock::now();
  auto matchResult = book.match(2, matchOrderId, side, price, quantity);
  auto end = std::chrono::high_resolution_clock::now();

  auto latency = std::chrono::duration<double, std::micro>(end - start).count();
  results.eventLogs.emplace_back(OperationType::MATCH, latency, matchOrderId,
                                 price, quantity, side);
  results.matches++;

  if (!matchResult.matches_.empty()) {
    std::ranges::for_each(matchResult.matches_,
                          [&activeOrdersMap](const auto &match) {
                            activeOrdersMap.erase(match.matchedOrderId_);
                          });
  }
}

auto runSimulation(
    stockex::engine::OrderBook &book,
    std::unordered_map<OrderId, ActiveOrderDetails> &activeOrdersMap,
    std::vector<OrderId> &activeOrdersVec, OrderId &nextMarketOrderId,
    const SimulationConfig &config, std::mt19937 &rng) -> SimulationResults {

  SimulationResults results;
  results.eventLogs.reserve(config.totalEvents);

  std::uniform_int_distribution actionDist(1, config.orderToTradeRatio);
  std::uniform_int_distribution addCancelDist(1, 100);
  std::uniform_int_distribution<Quantity> qtyDist(1, 100);
  std::normal_distribution<> priceDist(config.basePrice, config.priceStdDev);

  std::println("\n--- Starting simulation for {} events... ---",
               config.totalEvents);
  for (std::size_t i = 0; i < config.totalEvents; ++i) {
    const int eventType = actionDist(rng);

    if (eventType < config.orderToTradeRatio) {
      if (addCancelDist(rng) <= config.addProbabilityPercent) {
        handleAddOperation(book, activeOrdersMap, activeOrdersVec,
                             nextMarketOrderId, config, results, priceDist,
                             qtyDist, rng);
      } else {
        handleCancelOperation(book, activeOrdersMap, activeOrdersVec, results,
                                rng);
      }
    } else {
      const auto side = (i % 2 == 0) ? Side::SELL : Side::BUY;
      handleMatchOperation(book, activeOrdersMap, nextMarketOrderId, config,
                             results, qtyDist, side, rng);
    }
  }
  return results;
}

auto parseConfig(int argc, char **argv) -> SimulationConfig {
  if (argc != 5) {
    std::print(stderr,
               "Usage: {} <implementation_name> <scenario> <price_std_dev> "
               "<total_events> \n",
               argv[0]);
    std::print(stderr,
               "Scenarios: add_heavy, cancel_heavy, match_heavy, balanced\n");
    exit(1);
  }

  SimulationConfig config;
  config.implementationName = argv[1];
  config.scenarioName = argv[2];
  try {
    config.priceStdDev = std::stod(argv[3]);
    config.totalEvents = std::stoull(argv[4]);
  } catch (const std::invalid_argument &) {
    std::print(stderr, "Error: Invalid numeric argument for price_std_dev.\n");
    exit(1);
  }

  config.basePrice = BASE_PRICE;

  if (auto scenario = config.scenarioName; scenario == "add_heavy") {
    config.orderToTradeRatio = 50;
    config.addProbabilityPercent = 80;
    config.initialBookDepth = 100'000;
  } else if (scenario == "cancel_heavy") {
    config.orderToTradeRatio = 50;
    config.addProbabilityPercent = 20;
    config.initialBookDepth = 1'000'000;
  } else if (scenario == "match_heavy") {
    config.orderToTradeRatio = 5;
    config.addProbabilityPercent = 55;
    config.initialBookDepth = 1'000'000;
  } else if (scenario == "balanced") {
    config.orderToTradeRatio = 5;
    config.addProbabilityPercent = 60;
    config.initialBookDepth = 20'000;
  } else {
    std::print(stderr, "Unknown scenario: {}\n", scenario);
    exit(1);
  }

  return config;
}

int main(int argc, char **argv) {
  SimulationConfig config = parseConfig(argc, argv);

  std::string suffix =
      std::format("{}_{}_{}", config.implementationName, config.scenarioName,
                  static_cast<int>(config.priceStdDev));
  std::ofstream logFile(std::format("simulation_log_{}.txt", suffix));
  if (!logFile.is_open()) {
    std::print(stderr, "Fatal: Could not create log file.\n");
    return 1;
  }

  auto book = std::make_unique<stockex::engine::OrderBook>(1);
  std::mt19937 rng(42);

  std::unordered_map<OrderId, ActiveOrderDetails> activeOrdersMap;
  std::vector<OrderId> activeOrdersVec;
  OrderId nextMarketOrderId{};

  std::println("--- Pre-filling order book with {} orders... ---",
               config.initialBookDepth);
  prefillOrderBook(*book, rng, activeOrdersMap, activeOrdersVec,
                   nextMarketOrderId, config, logFile);
  std::println("Book pre-filled. Active orders: {}", activeOrdersVec.size());

  auto start = std::chrono::high_resolution_clock::now();
  auto results = runSimulation(*book, activeOrdersMap, activeOrdersVec,
                               nextMarketOrderId, config, rng);
  auto end = std::chrono::high_resolution_clock::now();

  auto simulationTime = std::chrono::duration<double>(end - start).count();

  std::println("\n--- Simulation Complete---");
  std::println("Time Elapsed: {}s", simulationTime);
  std::println("Adds: {}, Cancels: {}, Matches: {}", results.adds,
               results.cancels, results.matches);

  std::vector<double> addLatencies;
  std::vector<double> cancelLatencies;
  std::vector<double> matchLatencies;

  addLatencies.reserve(results.adds);
  cancelLatencies.reserve(results.cancels);
  matchLatencies.reserve(results.matches);

  for (const auto &log : results.eventLogs) {
    switch (log.type) {
      using enum OperationType;
    case ADD:
      addLatencies.push_back(log.latency);
      break;
    case CANCEL:
      cancelLatencies.push_back(log.latency);
      break;
    case MATCH:
      matchLatencies.push_back(log.latency);
      break;
    }
  }

  std::println("\n--- ADD Operation Metrics ---");
  printMetrics(addLatencies, results.adds);

  std::println("\n--- CANCEL Operation Metrics ---");
  printMetrics(cancelLatencies, results.cancels);

  std::println("\n--- MATCH Operation Metrics ---");
  printMetrics(matchLatencies, results.matches);

  std::println("\n--- Saving simulation logs... ---");

  saveLogsToFile(results, logFile);

  stockex::benchmarks::saveLatenciesToFile(
      addLatencies, std::format("latencies_add_{}.txt", suffix));
  stockex::benchmarks::saveLatenciesToFile(
      cancelLatencies, std::format("latencies_cancel_{}.txt", suffix));
  stockex::benchmarks::saveLatenciesToFile(
      matchLatencies, std::format("latencies_match_{}.txt", suffix));

  std::println("Data saved successfully.");

  return 0;
}
