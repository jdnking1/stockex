#pragma once

#include <array>
#include <expected>
#include <span>
#include <vector>

#include "models/basic_types.hpp"
#include "models/constants.hpp"
#include "models/price_level.hpp"

namespace stockex::engine {

enum class OrderBookError : uint8_t {
  OrderIdExhausted,
  OrderQueuePoolExhausted,
  InvalidOrderId,
};

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
  explicit OrderBook(models::InstrumentId instrument,
                     std::size_t maxOrders = models::MAX_NUM_ORDERS)
      : orders_{maxOrders}, instrument_{instrument} {
    freeList_.reserve(maxOrders);
    bids_.reserve(models::MAX_PRICE_LEVELS);
    asks_.reserve(models::MAX_PRICE_LEVELS);
  }

  OrderBook(const OrderBook &) = delete;
  OrderBook(OrderBook &&) = delete;
  OrderBook &operator=(const OrderBook &) = delete;
  OrderBook &operator=(OrderBook &&) = delete;

  [[nodiscard]] auto addOrder(models::ClientId clientId, models::Side side,
                              models::Price price,
                              models::Quantity quantity) noexcept
      -> std::expected<models::OrderId, OrderBookError>;

  auto removeOrder(models::OrderId orderId) noexcept
      -> std::expected<void, OrderBookError>;

  [[nodiscard]] MatchResultSet match(models::ClientId clientId,
                                     models::Side side, models::Price price,
                                     models::Quantity quantity) noexcept;

  [[nodiscard]] auto getOrder(models::OrderId orderId) const noexcept
      -> const models::OrderInfo & {
    return orders_[orderId];
  }

  [[nodiscard]] auto getPriceLevel(models::Price price) const noexcept
      -> const models::PriceLevel * {
    if (!bids_.empty() && price <= bids_.front().price_) {
      for (auto &pl : bids_)
        if (pl.price_ == price)
          return &pl;
    } else if (!asks_.empty() && price >= asks_.front().price_) {
      for (auto &pl : asks_)
        if (pl.price_ == price)
          return &pl;
    }
    return nullptr;
  }

  [[nodiscard]] auto getPriceLevel(models::Price price) noexcept
      -> models::PriceLevel * {
    if (!bids_.empty() && price <= bids_.front().price_) {
      for (auto &pl : bids_)
        if (pl.price_ == price)
          return &pl;
    } else if (!asks_.empty() && price >= asks_.front().price_) {
      for (auto &pl : asks_)
        if (pl.price_ == price)
          return &pl;
    }
    return nullptr;
  }

private:
  auto allocateOrderId() noexcept -> models::OrderId {
    if (!freeList_.empty()) {
      auto id = freeList_.back();
      freeList_.pop_back();
      return id;
    }
    return nextId_++;
  }

  [[nodiscard]] auto peekNextOrderId() const noexcept -> models::OrderId {
    if (!freeList_.empty()) {
      return freeList_.back();
    }
    return nextId_;
  }

  auto releaseOrderId(models::OrderId id) noexcept -> void {
    freeList_.push_back(id);
  }

  auto addPriceLevel(models::Side side, models::Price price) noexcept
      -> models::PriceLevel *;

  auto removePriceLevel(models::Price price, models::Side side) noexcept
      -> void;

  auto removeHeadOrder(models::PriceLevelVec &levels) noexcept -> void {
    releaseOrderId(levels.front().getFrontOrder()->orderId_);
    levels.front().popFrontOrder();
    if (levels.front().isEmpty()) {
      levels.erase(levels.begin());
    }
  }

  models::DefaultOrderQueue::Allocator orderQueueAllocator_{10000};

  models::PriceLevelVec bids_;
  models::PriceLevelVec asks_;

  std::vector<models::OrderInfo> orders_;
  std::vector<models::OrderId> freeList_;
  models::OrderId nextId_{0};

  std::array<MatchResult, models::MAX_MATCH_EVENTS> matchResults_{};

  models::InstrumentId instrument_{};
};
} // namespace stockex::engine
