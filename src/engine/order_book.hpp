#pragma once

#include <array>
#include <span>
#include <vector>

#include "models/basic_types.hpp"
#include "models/constants.hpp"
#include "models/order_queue.hpp"
#include "models/price_level.hpp"
#include "utils/memory_pool.hpp"

namespace stockex::engine {

struct MatchResult {
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
      : instrument_{instrument} {}

  OrderBook(const OrderBook &) = delete;
  OrderBook(OrderBook &&) = delete;
  OrderBook &operator=(const OrderBook &) = delete;
  OrderBook &operator=(OrderBook &&) = delete;

  auto addOrder(models::ClientId clientId, models::Side side,
                models::Price price, models::Quantity quantity) noexcept
      -> models::OrderId;

  void removeOrder(models::OrderId orderId) noexcept;

  [[nodiscard]] MatchResultSet match(models::ClientId clientId,
                                     models::Side side, models::Price price,
                                     models::Quantity quantity) noexcept;

  [[nodiscard]] auto getPriceIndex(models::Price price) const noexcept {
    return price % models::MAX_PRICE_LEVELS;
  }

  [[nodiscard]] auto getOrder(models::OrderId orderId) const noexcept
      -> const models::OrderInfo & {
    return orders_[orderId];
  }

  [[nodiscard]] auto getPriceLevel(models::Price price) const noexcept
      -> const models::PriceLevel * {
    return priceLevels_[getPriceIndex(price)];
  }

  [[nodiscard]] auto getPriceLevel(models::Price price) noexcept
      -> models::PriceLevel * {
    return priceLevels_[getPriceIndex(price)];
  }

private:
  auto allocateOrderId() noexcept -> models::OrderId {
    if (!freeList_.empty()) {
      auto id = freeList_.back();
      freeList_.pop_back();
      return id;
    }
    auto id = nextId_++;
    orders_.emplace_back();
    return id;
  }

  auto releaseOrderId(models::OrderId id) noexcept -> void {
    freeList_.push_back(id);
  }

  auto addPriceLevel(models::Side side, models::Price price) noexcept
      -> models::PriceLevel *;

  auto removePriceLevel(models::PriceLevel *priceLevel) noexcept -> void;

  auto removeHeadOrder(models::PriceLevel *priceLevel) noexcept -> void {
    priceLevel->popFrontOrder();
    if (priceLevel->isEmpty()) {
      removePriceLevel(priceLevel);
    }
  }

  auto insertPriceLevelBefore(models::PriceLevel *current,
                              models::PriceLevel *newPriceLevel) noexcept
      -> void;

  models::PriceLevel *bestBid_{};
  models::PriceLevel *bestAsk_{};

  models::PriceLevelMap priceLevels_{};

  std::vector<models::OrderInfo> orders_;
  std::vector<models::OrderId> freeList_;
  models::OrderId nextId_{0};

  std::array<MatchResult, models::MAX_MATCH_EVENTS> matchResults_{};

  utils::MemoryPool<models::PriceLevel> priceLevelAllocator_{
      models::MAX_PRICE_LEVELS};
  models::DefaultOrderQueue::Allocator OrderQueueAllocator{10000};

  models::InstrumentId instrument_{};
};
} // namespace stockex::engine
