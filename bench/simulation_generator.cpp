#include "engine/order_book.hpp"
#include "models/basic_types.hpp"
#include "simulation_event.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <print>
#include <random>
#include <unordered_map>
#include <vector>

namespace stockex::benchmarks {

constexpr models::Price BASE_PRICE = 5000;

struct SimulationConfig {
  std::string scenarioName;
  std::size_t totalEvents;
  std::size_t initialBookDepth;
  int orderToTradeRatio;
  int addProbabilityPercent;
  models::Price basePrice;
  double priceStdDev;
};

struct ActiveOrderDetails {
  models::ClientId clientId;
  models::Price price;
  models::Quantity quantity;
  models::Side side;
};

auto parseConfig(int argc, char **argv) -> SimulationConfig {
  if (argc != 4) {
    std::print(stderr,
               "Usage: {} <implementation_name> <scenario> <price_std_dev> "
               "<total_events> \n",
               argv[0]);
    std::print(stderr,
               "Scenarios: add_heavy, cancel_heavy, match_heavy, balanced\n");
    exit(1);
  }

  SimulationConfig config;
  config.scenarioName = argv[1];

  try {
    config.priceStdDev = std::stod(argv[2]);
    config.totalEvents = std::stoull(argv[3]);
  } catch (const std::invalid_argument &) {
    std::print(stderr, "Error: Invalid numeric argument for price_std_dev.\n");
    exit(1);
  }

  config.basePrice = BASE_PRICE;

  if (auto scenario = config.scenarioName; scenario == "add_heavy") {
    config.orderToTradeRatio = 50;
    config.addProbabilityPercent = 80;
    config.initialBookDepth = 10'000;
  } else if (scenario == "cancel_heavy") {
    config.orderToTradeRatio = 50;
    config.addProbabilityPercent = 20;
    config.initialBookDepth = 25'000;
  } else if (scenario == "match_heavy") {
    config.orderToTradeRatio = 5;
    config.addProbabilityPercent = 55;
    config.initialBookDepth = 10'000;
  } else if (scenario == "balanced") {
    config.orderToTradeRatio = 5;
    config.addProbabilityPercent = 60;
    config.initialBookDepth = 10'000;
  } else {
    std::print(stderr, "Unknown scenario: {}\n", scenario);
    exit(1);
  }

  return config;
}

auto handleAddOperation(
    std::vector<SimulationEvent> &events,
    std::unordered_map<models::OrderId, ActiveOrderDetails> &activeOrdersMap,
    std::vector<models::OrderId> &activeOrdersVec,
    models::OrderId &nextMarketOrderId, const SimulationConfig &config,
    std::normal_distribution<> &priceDist,
    std::uniform_int_distribution<models::Quantity> &qtyDist, std::mt19937 &rng,
    stockex::engine::OrderBook &book) -> void {
  const auto price = static_cast<models::Price>(std::round(priceDist(rng)));
  const auto quantity = qtyDist(rng);
  const models::Side side =
      (price < config.basePrice) ? models::Side::BUY : models::Side::SELL;
  const models::ClientId clientId = 1;
  const models::OrderId newOrderId = nextMarketOrderId++;
  activeOrdersMap[newOrderId] = {clientId, price, quantity, side};
  activeOrdersVec.push_back(newOrderId);
  events.emplace_back(newOrderId, price, quantity, side, EventType::ADD,
                      clientId);
  book.addOrder(clientId, newOrderId, newOrderId, side, price, quantity);
}

auto handleCancelOperation(
    std::vector<SimulationEvent> &events,
    std::unordered_map<models::OrderId, ActiveOrderDetails> &activeOrdersMap,
    std::vector<models::OrderId> &activeOrdersVec, std::mt19937 &rng,
    stockex::engine::OrderBook &book) -> void {
  for (auto i = 0; i < 3; i++) {
    if (activeOrdersVec.empty()) {
      return;
    }

    std::uniform_int_distribution<std::size_t> cancelIndexDist(
        0, activeOrdersVec.size() - 1);

    auto randomIndex = cancelIndexDist(rng);
    models::OrderId orderToCancel = activeOrdersVec[randomIndex];

    activeOrdersVec[randomIndex] = activeOrdersVec.back();
    activeOrdersVec.pop_back();

    auto it = activeOrdersMap.find(orderToCancel);
    if (it != activeOrdersMap.end()) {
      const auto orderDetails = it->second;
      activeOrdersMap.erase(it);
      SimulationEvent event;
      event.type = EventType::CANCEL;
      event.clientId = orderDetails.clientId;
      event.orderId = orderToCancel;
      events.push_back(event);
      book.removeOrder(event.clientId, event.orderId);
      return;
    }
  }
}

auto handleMatchOperation(
    std::vector<SimulationEvent> &events,
    std::unordered_map<models::OrderId, ActiveOrderDetails> &activeOrdersMap,
    models::OrderId &nextMarketOrderId, const SimulationConfig &config,
    std::uniform_int_distribution<models::Quantity> &qtyDist, models::Side side,
    std::mt19937 &rng, stockex::engine::OrderBook &book) -> void {
  const models::Price price = (side == models::Side::SELL)
                                  ? (config.basePrice - 20)
                                  : (config.basePrice + 20);
  const models::Quantity quantity = qtyDist(rng) * 5;
  const models::OrderId matchOrderId = nextMarketOrderId++;

  events.emplace_back(matchOrderId, price, quantity, side, EventType::MATCH, 2);

  auto matchResult = book.match(2, matchOrderId, side, price, quantity);

  if (!matchResult.matches_.empty()) {
    std::ranges::for_each(matchResult.matches_,
                          [&activeOrdersMap](const auto &match) {
                            activeOrdersMap.erase(match.matchedOrderId_);
                          });
  }
}

auto generatePrefillData(
    std::vector<SimulationEvent> &events, std::mt19937 &rng,
    std::unordered_map<models::OrderId, ActiveOrderDetails> &activeOrdersMap,
    std::vector<models::OrderId> &activeOrdersVec,
    models::OrderId &nextMarketOrderId, const SimulationConfig &config,
    stockex::engine::OrderBook &book) -> void {
  std::normal_distribution<> priceDist(config.basePrice, config.priceStdDev);
  std::uniform_int_distribution<models::Quantity> qtyDist(1, 100);

  for (std::size_t i = 0; i < config.initialBookDepth; ++i) {
    const auto price = static_cast<models::Price>(std::round(priceDist(rng)));
    const auto quantity = qtyDist(rng);
    const auto side =
        (price < config.basePrice) ? models::Side::BUY : models::Side::SELL;
    activeOrdersMap[nextMarketOrderId] = {1, price, quantity, side};
    activeOrdersVec.push_back(nextMarketOrderId);
    events.emplace_back(nextMarketOrderId, price, quantity, side,
                        EventType::PREFILL, 1);
    book.addOrder(1, nextMarketOrderId, nextMarketOrderId, side, price,
                  quantity);
    nextMarketOrderId++;
  }
}

auto generateSimulationData(
    std::vector<SimulationEvent> &events, std::mt19937 &rng,
    std::unordered_map<models::OrderId, ActiveOrderDetails> &activeOrdersMap,
    std::vector<models::OrderId> &activeOrdersVec,
    models::OrderId &nextMarketOrderId, const SimulationConfig &config,
    stockex::engine::OrderBook &book) -> void {
  std::uniform_int_distribution actionDist(1, config.orderToTradeRatio);
  std::uniform_int_distribution addCancelDist(1, 100);
  std::uniform_int_distribution<models::Quantity> qtyDist(1, 100);
  std::normal_distribution<> priceDist(config.basePrice, config.priceStdDev);

  for (std::size_t i = 0; i < config.totalEvents; ++i) {
    const int eventType = actionDist(rng);

    if (eventType < config.orderToTradeRatio) {
      if (addCancelDist(rng) <= config.addProbabilityPercent) {
        handleAddOperation(events, activeOrdersMap, activeOrdersVec,
                           nextMarketOrderId, config, priceDist, qtyDist, rng,
                           book);
      } else {
        handleCancelOperation(events, activeOrdersMap, activeOrdersVec, rng,
                              book);
      }
    } else {
      const auto side = (i % 2 == 0) ? models::Side::SELL : models::Side::BUY;
      handleMatchOperation(events, activeOrdersMap, nextMarketOrderId, config,
                           qtyDist, side, rng, book);
    }
  }
}

}; // namespace stockex::benchmarks

int main(int argc, char **argv) {
  stockex::benchmarks::SimulationConfig config =
      stockex::benchmarks::parseConfig(argc, argv);
  std::ofstream simulation_file(
      std::format("simulation_{}_{}_{}.bin", config.scenarioName,
                  config.priceStdDev, config.totalEvents));
  if (!simulation_file.is_open()) {
    std::print(stderr, "Fatal: Could not create log file.\n");
    return 1;
  }

  std::println(" --- generating simulation for scenario {} price_std_dev {}  "
               "events number {} ---",
               config.scenarioName, config.priceStdDev, config.totalEvents);
  std::mt19937 rng(42);
  auto book = std::make_unique<stockex::engine::OrderBook>(1);
  std::unordered_map<stockex::models::OrderId,
                     stockex::benchmarks::ActiveOrderDetails>
      activeOrdersMap{};
  std::vector<stockex::models::OrderId> activeOrdersVec{};
  std::vector<stockex::benchmarks::SimulationEvent> events;
  events.reserve(config.totalEvents);

  stockex::models::OrderId nextMarketOrderId{};

  generatePrefillData(events, rng, activeOrdersMap, activeOrdersVec,
                      nextMarketOrderId, config, *book);

  generateSimulationData(events, rng, activeOrdersMap, activeOrdersVec,
                         nextMarketOrderId, config, *book);

  simulation_file.write(reinterpret_cast<const char *>(events.data()),
                        events.size() *
                            sizeof(stockex::benchmarks::SimulationEvent));

  std::println("\n--- simulation generated ---");

  return 0;
}