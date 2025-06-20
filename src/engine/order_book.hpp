#pragma once

#include <array>

#include "models/basic_types.hpp"
#include "models/constants.hpp"
#include "models/order.hpp"
#include "utils/memory_pool.hpp"

namespace stockex::engine {

struct MatchResult {
  models::OrderId incomingOrderId_{};
  models::OrderId matchedOrderId_{};
  models::Price price_{};
  models::Quantity quantity_{};
  models::Quantity matchedOrderRemainingQty_{};
  models::ClientId incomingClientId_{};
  models::ClientId matchedClientId_{};
  models::Side incomingOrderSide_;
  models::Side matchedOrderSide_;
};

struct MatchResultSet {
  std::span<MatchResult> matches_{};
  models::Quantity remainingQuantity_{};
  models::InstrumentId instrument_{};
  bool overflow_{false};
};

class OrderBook {
public:
  explicit OrderBook(models::InstrumentId instrument)
      : instrument_{instrument} {
    clientOrders_.fill({models::MAX_NUM_ORDERS, nullptr});
  }

  OrderBook(const OrderBook &) = delete;
  OrderBook(OrderBook &&) = delete;
  OrderBook &operator=(const OrderBook &) = delete;
  OrderBook &operator=(OrderBook &&) = delete;

  auto getPriceIndex(models::Price price) const noexcept {
    return price % models::MAX_PRICE_LEVELS;
  }

  auto getOrder(models::ClientId clientId,
                models::OrderId orderId) const noexcept
      -> const models::Order * {
    return clientOrders_[clientId][orderId];
  }

  auto modifyOrder(models::ClientId clientId, models::OrderId orderId,
                   models::Quantity qty) noexcept -> void {
    clientOrders_[clientId][orderId]->qty_ = qty;
  };

  auto getPriceLevel(models::Price price) const noexcept
      -> models::PriceLevel * {
    return priceLevels_[getPriceIndex(price)];
  }

  auto addOrder(models::ClientId, models::OrderId, models::OrderId,
                models::Side, models::Price, models::Quantity) noexcept -> void;

  auto removeOrder(models::ClientId, models::OrderId) noexcept -> void;

  auto addPriceLevel(models::Order *) noexcept -> void;

  auto removePriceLevel(models::Side, models::Price) noexcept -> void;

  auto match(models::ClientId clientId, models::OrderId orderId,
             models::Side side, models::Price price,
             models::Quantity quantity) noexcept -> MatchResultSet;

private:
  models::InstrumentId instrument_{};
  models::PriceLevel *bestBid_{};
  models::PriceLevel *bestAsk_{};
  models::PriceLevelMap priceLevels_{};
  models::ClientOrderMap clientOrders_{};
  std::array<MatchResult, models::MAX_MATCH_EVENTS> matchResults_{};
  utils::MemoryPool<models::Order> orderAllocator_{models::MAX_NUM_ORDERS};
  utils::MemoryPool<models::PriceLevel> priceLevelAllocator_{
      models::MAX_PRICE_LEVELS};
};
} // namespace stockex::engine