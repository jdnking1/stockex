#include "engine/order_book.hpp"
#include "models/basic_types.hpp"
#include "simulation_event.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <print>
#include <random>
#include <vector>

namespace stockex::benchmarks {

constexpr models::Price BASE_PRICE = 5000;
constexpr models::Price MIN_PRICE = 3000;
constexpr models::Price MAX_PRICE = 7000;

// ---------------------------------------------------------------------------
// Active order tracking (per-participant)
// ---------------------------------------------------------------------------
struct ActiveOrder {
  models::OrderId id;
  models::Price price;
  models::Quantity qty;
  models::Side side;
};

// ---------------------------------------------------------------------------
// Market state
// ---------------------------------------------------------------------------
struct MarketState {
  double midPrice = BASE_PRICE;
  int spread = 2; // in ticks
  int baseSpread = 2;
  int maxSpread = 5;
  double volatility = 0.3; // probability of ±1 tick walk per update
};

// Safely compute price relative to mid, clamped to valid range
auto priceFromMid(int mid, int offset, models::Side side) -> models::Price {
  int raw = (side == models::Side::BUY) ? (mid - offset) : (mid + offset);
  return static_cast<models::Price>(
      std::clamp(raw, static_cast<int>(MIN_PRICE), static_cast<int>(MAX_PRICE)));
}

auto updateMidPrice(MarketState &ms, std::mt19937 &rng,
                    std::uniform_real_distribution<double> &prob,
                    std::uniform_int_distribution<int> &spreadNoise) -> void {
  // Random walk
  double step = 0.0;
  double r = prob(rng);
  if (r < ms.volatility)
    step = 1.0;
  else if (r < ms.volatility * 2.0)
    step = -1.0;

  // Mean reversion toward BASE_PRICE
  double dist = ms.midPrice - BASE_PRICE;
  if (std::abs(dist) > 200.0)
    step -= 0.1 * (dist / std::abs(dist));

  ms.midPrice += step;
  ms.midPrice = std::clamp(ms.midPrice, static_cast<double>(MIN_PRICE),
                           static_cast<double>(MAX_PRICE));

  // Spread jitter
  ms.spread =
      std::clamp(ms.baseSpread + spreadNoise(rng), 1, ms.maxSpread);
}

// ---------------------------------------------------------------------------
// Participant state structs
// ---------------------------------------------------------------------------
struct MarketMakerState {
  models::ClientId clientId;
  std::vector<ActiveOrder> orders;
  int numLevels;
  int ordersPerLevel;
  double cancelReplaceProb;
};

struct AggressiveTraderState {
  models::ClientId clientId;
  std::vector<ActiveOrder> orders;
  double matchProb;
  double limitProb;
  int tickCounter = 0;
};

struct PassiveTraderState {
  models::ClientId clientId;
  std::vector<ActiveOrder> orders;
  double addProb;
  double cancelProb;
};

// ---------------------------------------------------------------------------
// Scenario configuration
// ---------------------------------------------------------------------------
struct ScenarioConfig {
  std::string name;
  std::size_t totalEvents;
  std::size_t prefillDepth;
  int baseSpread;
  int maxSpread;
  double volatility;
  std::vector<MarketMakerState> marketMakers;
  std::vector<AggressiveTraderState> aggressiveTraders;
  std::vector<PassiveTraderState> passiveTraders;
};

auto makeScenario(const std::string &name, std::size_t totalEvents)
    -> ScenarioConfig {
  ScenarioConfig cfg;
  cfg.name = name;
  cfg.totalEvents = totalEvents;

  if (name == "normal") {
    cfg.prefillDepth = 10'000;
    cfg.baseSpread = 2;
    cfg.maxSpread = 4;
    cfg.volatility = 0.3;
    cfg.marketMakers = {
        {1, {}, 3, 3, 0.60},
        {2, {}, 3, 3, 0.60},
    };
    cfg.aggressiveTraders = {
        {4, {}, 0.05, 0.02},
        {5, {}, 0.05, 0.02},
    };
    cfg.passiveTraders = {
        {7, {}, 0.10, 0.005},
        {8, {}, 0.10, 0.005},
    };
  } else if (name == "hft") {
    cfg.prefillDepth = 20'000;
    cfg.baseSpread = 1;
    cfg.maxSpread = 3;
    cfg.volatility = 0.2;
    cfg.marketMakers = {
        {1, {}, 5, 5, 0.92},
        {2, {}, 5, 5, 0.92},
        {3, {}, 5, 5, 0.92},
    };
    cfg.aggressiveTraders = {
        {4, {}, 0.01, 0.005},
    };
    cfg.passiveTraders = {
        {7, {}, 0.03, 0.002},
    };
  } else if (name == "volatile") {
    cfg.prefillDepth = 10'000;
    cfg.baseSpread = 3;
    cfg.maxSpread = 6;
    cfg.volatility = 0.5;
    cfg.marketMakers = {
        {1, {}, 2, 3, 0.40},
    };
    cfg.aggressiveTraders = {
        {4, {}, 0.15, 0.05},
        {5, {}, 0.15, 0.05},
        {6, {}, 0.12, 0.04},
    };
    cfg.passiveTraders = {
        {7, {}, 0.08, 0.003},
    };
  } else if (name == "thin") {
    cfg.prefillDepth = 5'000;
    cfg.baseSpread = 3;
    cfg.maxSpread = 6;
    cfg.volatility = 0.25;
    cfg.marketMakers = {
        {1, {}, 2, 2, 0.70},
    };
    cfg.aggressiveTraders = {
        {4, {}, 0.03, 0.01},
    };
    cfg.passiveTraders = {
        {7, {}, 0.05, 0.003},
    };
  } else {
    std::print(stderr, "Unknown scenario: {}\n", name);
    std::print(stderr, "Available: normal, hft, volatile, thin\n");
    exit(1);
  }

  return cfg;
}

// ---------------------------------------------------------------------------
// Helper: emit an ADD event and track it
// ---------------------------------------------------------------------------
auto emitAdd(std::vector<ActiveOrder> &tracker,
             std::vector<SimulationEvent> &events,
             engine::OrderBook &book, models::ClientId clientId,
             models::Side side, models::Price price, models::Quantity qty,
             std::size_t &eventCount) -> bool {
  auto result = book.addOrder(clientId, side, price, qty);
  if (!result.has_value())
    return false; // silently skip (e.g., pool exhausted)
  auto orderId = *result;
  tracker.push_back({orderId, price, qty, side});
  events.emplace_back(orderId, price, qty, side, EventType::ADD, clientId);
  ++eventCount;
  return true;
}

// ---------------------------------------------------------------------------
// Helper: emit a CANCEL event
// ---------------------------------------------------------------------------
auto emitCancel(std::vector<ActiveOrder> &tracker, std::size_t index,
                std::vector<SimulationEvent> &events,
                engine::OrderBook &book, models::ClientId clientId,
                std::size_t &eventCount) -> bool {
  auto &order = tracker[index];
  auto result = book.removeOrder(order.id);
  if (!result.has_value())
    return false;
  events.emplace_back(order.id, models::Price{}, models::Quantity{},
                      models::Side::INVALID, EventType::CANCEL, clientId);
  ++eventCount;
  // Swap-and-pop removal
  tracker[index] = tracker.back();
  tracker.pop_back();
  return true;
}

// ---------------------------------------------------------------------------
// Market Maker behavior
// ---------------------------------------------------------------------------
auto marketMakerAct(MarketMakerState &mm, int mid,
                    engine::OrderBook &book,
                    std::vector<SimulationEvent> &events, std::mt19937 &rng,
                    std::uniform_real_distribution<double> &prob,
                    std::uniform_int_distribution<models::Quantity> &mmQtyDist,
                    std::size_t &eventCount, std::size_t maxEvents,
                    int halfSpread) -> void {
  if (prob(rng) >= mm.cancelReplaceProb)
    return;

  // Cancel all existing quotes
  while (!mm.orders.empty() && eventCount < maxEvents) {
    emitCancel(mm.orders, mm.orders.size() - 1, events, book, mm.clientId,
               eventCount);
  }

  // Re-place quotes at updated prices (signed arithmetic to avoid underflow)
  for (int level = 0; level < mm.numLevels && eventCount < maxEvents;
       ++level) {
    auto bidPrice = priceFromMid(mid, halfSpread + level, models::Side::BUY);
    auto askPrice = priceFromMid(mid, halfSpread + level, models::Side::SELL);

    for (int i = 0; i < mm.ordersPerLevel && eventCount < maxEvents; ++i) {
      auto qty = mmQtyDist(rng);
      emitAdd(mm.orders, events, book, mm.clientId, models::Side::BUY,
              bidPrice, qty, eventCount);
      if (eventCount >= maxEvents)
        return;
      emitAdd(mm.orders, events, book, mm.clientId, models::Side::SELL,
              askPrice, qty, eventCount);
    }
  }
}

// ---------------------------------------------------------------------------
// Aggressive Trader behavior
// ---------------------------------------------------------------------------
auto aggressiveTraderAct(AggressiveTraderState &at, int mid,
                         engine::OrderBook &book,
                         std::vector<SimulationEvent> &events,
                         std::mt19937 &rng,
                         std::uniform_real_distribution<double> &prob,
                         std::uniform_int_distribution<models::Quantity> &aggQtyDist,
                         std::uniform_int_distribution<int> &nearSpreadDist,
                         std::size_t &eventCount, std::size_t maxEvents) -> void {
  if (eventCount >= maxEvents)
    return;

  // Marketable order (match)
  if (prob(rng) < at.matchProb) {
    at.tickCounter++;
    auto side =
        (at.tickCounter % 2 == 0) ? models::Side::BUY : models::Side::SELL;
    // BUY crosses the ask → high price; SELL crosses the bid → low price
    auto price = priceFromMid(mid, 30, side == models::Side::BUY
                                           ? models::Side::SELL  // mid + 30
                                           : models::Side::BUY); // mid - 30
    auto qty = aggQtyDist(rng);

    events.emplace_back(0, price, qty, side, EventType::MATCH,
                        at.clientId);
    [[maybe_unused]] auto matchResult =
        book.match(at.clientId, side, price, qty);
    ++eventCount;
    return;
  }

  // Limit order near the spread
  if (prob(rng) < at.limitProb) {
    auto side =
        (prob(rng) < 0.5) ? models::Side::BUY : models::Side::SELL;
    auto offset = nearSpreadDist(rng);
    auto price = priceFromMid(mid, offset, side);
    auto qty = aggQtyDist(rng);
    emitAdd(at.orders, events, book, at.clientId, side, price, qty,
            eventCount);
  }
}

// ---------------------------------------------------------------------------
// Passive Trader behavior
// ---------------------------------------------------------------------------
auto passiveTraderAct(PassiveTraderState &pt, int mid,
                      engine::OrderBook &book,
                      std::vector<SimulationEvent> &events,
                      std::mt19937 &rng,
                      std::uniform_real_distribution<double> &prob,
                      std::uniform_int_distribution<models::Quantity> &passiveQtyDist,
                      std::uniform_int_distribution<int> &depthDist,
                      std::size_t &eventCount, std::size_t maxEvents) -> void {
  if (eventCount >= maxEvents)
    return;

  // Place deep limit order
  if (prob(rng) < pt.addProb) {
    auto side =
        (prob(rng) < 0.5) ? models::Side::BUY : models::Side::SELL;
    auto offset = depthDist(rng);
    auto price = priceFromMid(mid, offset, side);
    auto qty = passiveQtyDist(rng);
    emitAdd(pt.orders, events, book, pt.clientId, side, price, qty,
            eventCount);
  }

  // Occasionally cancel an existing order
  if (!pt.orders.empty() && prob(rng) < pt.cancelProb &&
      eventCount < maxEvents) {
    std::uniform_int_distribution<std::size_t> idxDist(
        0, pt.orders.size() - 1);
    auto idx = idxDist(rng);
    emitCancel(pt.orders, idx, events, book, pt.clientId, eventCount);
  }
}

// ---------------------------------------------------------------------------
// Prefill: build initial book depth
// ---------------------------------------------------------------------------
auto generatePrefill(std::vector<SimulationEvent> &events, std::mt19937 &rng,
                     engine::OrderBook &book, std::size_t depth) -> bool {
  std::normal_distribution<> priceDist(BASE_PRICE, 30.0);
  std::uniform_int_distribution<models::Quantity> qtyDist(1, 100);

  constexpr models::ClientId prefillClientId = 10;
  for (std::size_t i = 0; i < depth; ++i) {
    auto price =
        static_cast<models::Price>(std::clamp(static_cast<int>(std::round(priceDist(rng))),
                                              static_cast<int>(MIN_PRICE),
                                              static_cast<int>(MAX_PRICE)));
    auto qty = qtyDist(rng);
    auto side =
        (price < BASE_PRICE) ? models::Side::BUY : models::Side::SELL;
    auto result = book.addOrder(prefillClientId, side, price, qty);
    if (!result.has_value()) {
      std::print(stderr, "Error: addOrder failed during prefill at {}\n", i);
      return false;
    }
    auto orderId = *result;
    events.emplace_back(orderId, price, qty, side, EventType::PREFILL,
                        prefillClientId);
  }
  return true;
}

// ---------------------------------------------------------------------------
// Main simulation loop
// ---------------------------------------------------------------------------
auto runSimulation(ScenarioConfig &cfg, engine::OrderBook &book,
                   std::vector<SimulationEvent> &events,
                   std::mt19937 &rng) -> bool {
  MarketState ms;
  ms.baseSpread = cfg.baseSpread;
  ms.maxSpread = cfg.maxSpread;
  ms.spread = cfg.baseSpread;
  ms.volatility = cfg.volatility;

  // Pre-allocate participant order vectors
  for (auto &mm : cfg.marketMakers)
    mm.orders.reserve(mm.numLevels * mm.ordersPerLevel * 2);
  for (auto &at : cfg.aggressiveTraders)
    at.orders.reserve(64);
  for (auto &pt : cfg.passiveTraders)
    pt.orders.reserve(256);

  // Hoisted distributions (constructed once, reused every tick)
  std::uniform_real_distribution<double> prob(0.0, 1.0);
  std::uniform_int_distribution<int> spreadNoise(-1, 1);
  std::uniform_int_distribution<models::Quantity> mmQtyDist(1, 20);
  std::uniform_int_distribution<models::Quantity> aggQtyDist(1, 50);
  std::uniform_int_distribution<models::Quantity> passiveQtyDist(5, 100);
  std::uniform_int_distribution<int> nearSpreadDist(1, 5);
  std::uniform_int_distribution<int> depthDist(10, 49);

  std::size_t eventCount = 0;
  std::size_t maxEvents = cfg.totalEvents;

  while (eventCount < maxEvents) {
    updateMidPrice(ms, rng, prob, spreadNoise);
    auto mid = static_cast<int>(std::round(ms.midPrice));
    auto halfSpread = std::max(ms.spread / 2, 1);

    for (auto &mm : cfg.marketMakers) {
      marketMakerAct(mm, mid, book, events, rng, prob, mmQtyDist,
                     eventCount, maxEvents, halfSpread);
      if (eventCount >= maxEvents)
        break;
    }

    for (auto &at : cfg.aggressiveTraders) {
      aggressiveTraderAct(at, mid, book, events, rng, prob, aggQtyDist,
                          nearSpreadDist, eventCount, maxEvents);
      if (eventCount >= maxEvents)
        break;
    }

    for (auto &pt : cfg.passiveTraders) {
      passiveTraderAct(pt, mid, book, events, rng, prob, passiveQtyDist,
                       depthDist, eventCount, maxEvents);
      if (eventCount >= maxEvents)
        break;
    }
  }

  return true;
}

} // namespace stockex::benchmarks

int main(int argc, char **argv) {
  if (argc != 3) {
    std::print(stderr, "Usage: {} <scenario> <total_events>\n", argv[0]);
    std::print(stderr, "Scenarios: normal, hft, volatile, thin\n");
    return 1;
  }

  std::string scenario = argv[1];
  std::size_t totalEvents;
  try {
    totalEvents = std::stoull(argv[2]);
  } catch (...) {
    std::print(stderr, "Error: invalid total_events\n");
    return 1;
  }

  auto cfg = stockex::benchmarks::makeScenario(scenario, totalEvents);

  std::ofstream outFile(
      std::format("simulation_{}_{}.bin", cfg.name, cfg.totalEvents),
      std::ios::binary);
  if (!outFile.is_open()) {
    std::print(stderr, "Fatal: Could not create output file.\n");
    return 1;
  }

  std::println("--- Generating {} scenario, {} events ---", cfg.name,
               cfg.totalEvents);

  std::mt19937 rng(42);
  auto book = std::make_unique<stockex::engine::OrderBook>(1);

  std::vector<stockex::benchmarks::SimulationEvent> events;
  // Market makers generate cancel+add pairs, so actual event count can be ~2x
  events.reserve(cfg.prefillDepth + cfg.totalEvents * 2);

  // Phase 1: Prefill
  if (!stockex::benchmarks::generatePrefill(events, rng, *book,
                                            cfg.prefillDepth)) {
    std::print(stderr, "Fatal: prefill failed.\n");
    return 1;
  }
  std::println("Prefill: {} orders placed.", cfg.prefillDepth);

  // Phase 2: Simulation
  if (!stockex::benchmarks::runSimulation(cfg, *book, events, rng)) {
    std::print(stderr, "Fatal: simulation failed.\n");
    return 1;
  }

  // Summary statistics
  std::size_t addCount = 0, cancelCount = 0, matchCount = 0, prefillCount = 0;
  for (const auto &e : events) {
    switch (e.type) {
    case stockex::benchmarks::EventType::ADD:
      ++addCount;
      break;
    case stockex::benchmarks::EventType::CANCEL:
      ++cancelCount;
      break;
    case stockex::benchmarks::EventType::MATCH:
      ++matchCount;
      break;
    case stockex::benchmarks::EventType::PREFILL:
      ++prefillCount;
      break;
    }
  }

  auto simTotal =
      static_cast<double>(addCount + cancelCount + matchCount);
  std::println("\n--- Summary (excluding {} prefill events) ---", prefillCount);
  if (simTotal > 0) {
    std::println("  ADD:    {:>8}  ({:.1f}%)", addCount,
                 100.0 * addCount / simTotal);
    std::println("  CANCEL: {:>8}  ({:.1f}%)", cancelCount,
                 100.0 * cancelCount / simTotal);
    std::println("  MATCH:  {:>8}  ({:.1f}%)", matchCount,
                 100.0 * matchCount / simTotal);
  }
  std::println("  Total:  {:>8}", addCount + cancelCount + matchCount);

  // Write binary
  outFile.write(reinterpret_cast<const char *>(events.data()),
                static_cast<std::streamsize>(
                    events.size() *
                    sizeof(stockex::benchmarks::SimulationEvent)));

  std::println("\nWrote {} events to simulation_{}_{}.bin", events.size(),
               cfg.name, cfg.totalEvents);

  return 0;
}
