#pragma once

#include <array>
#include <expected>
#include <span>
#include <vector>

#include "models/basic_types.hpp"
#include "models/constants.hpp"
#include "models/order_queue.hpp"
#include "models/price_level.hpp"
#include "utils/memory_pool.hpp"

namespace stockex::engine {

enum class OrderBookError : uint8_t {
  OrderIdExhausted,
  PriceLevelPoolExhausted,
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
    auto idx = price % models::MAX_PRICE_LEVELS;
    while (priceLevels_[idx] != nullptr) {
      if (priceLevels_[idx]->price_ == price)
        return priceLevels_[idx];
      idx = (idx + 1) % models::MAX_PRICE_LEVELS;
    }
    return nullptr;
  }

  [[nodiscard]] auto getPriceLevel(models::Price price) noexcept
      -> models::PriceLevel * {
    auto idx = price % models::MAX_PRICE_LEVELS;
    while (priceLevels_[idx] != nullptr) {
      if (priceLevels_[idx]->price_ == price)
        return priceLevels_[idx];
      idx = (idx + 1) % models::MAX_PRICE_LEVELS;
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

  auto removePriceLevel(models::PriceLevel *priceLevel) noexcept -> void;

  auto removeHeadOrder(models::PriceLevel *priceLevel) noexcept -> void {
    releaseOrderId(priceLevel->getFrontOrder()->orderId_);
    priceLevel->popFrontOrder();
    if (priceLevel->isEmpty()) {
      removePriceLevel(priceLevel);
    }
  }

  auto insertPriceLevelBefore(models::PriceLevel *current,
                              models::PriceLevel *newPriceLevel) noexcept
      -> void;

  /// Insert pointer into hash table using linear probing.
  auto hashInsert(models::PriceLevel *pl) noexcept -> void {
    auto idx = pl->price_ % models::MAX_PRICE_LEVELS;
    while (priceLevels_[idx] != nullptr) {
      idx = (idx + 1) % models::MAX_PRICE_LEVELS;
    }
    priceLevels_[idx] = pl;
  }

  /// Remove entry from hash table using backward-shift deletion.
  auto hashRemove(models::Price price) noexcept -> void {
    auto idx = price % models::MAX_PRICE_LEVELS;
    while (priceLevels_[idx] != nullptr && priceLevels_[idx]->price_ != price) {
      idx = (idx + 1) % models::MAX_PRICE_LEVELS;
    }
    if (priceLevels_[idx] == nullptr)
      return;

    // Backward-shift deletion: fill the gap by shifting subsequent
    // entries that would probe past the removed slot.
    priceLevels_[idx] = nullptr;
    auto next = (idx + 1) % models::MAX_PRICE_LEVELS;
    while (priceLevels_[next] != nullptr) {
      auto natural = priceLevels_[next]->price_ % models::MAX_PRICE_LEVELS;
      // Check if 'next' sits at or after its natural slot relative to 'idx'.
      // If the gap at 'idx' is between natural and next (circularly),
      // then this entry needs to shift back.
      bool needsShift = (next > idx)
                            ? (natural <= idx || natural > next)
                            : (natural <= idx && natural > next);
      if (needsShift) {
        priceLevels_[idx] = priceLevels_[next];
        priceLevels_[next] = nullptr;
        idx = next;
      }
      next = (next + 1) % models::MAX_PRICE_LEVELS;
    }
  }

  models::PriceLevel *bestBid_{};
  models::PriceLevel *bestAsk_{};

  models::PriceLevelMap priceLevels_{};

  std::vector<models::OrderInfo> orders_;
  std::vector<models::OrderId> freeList_;
  models::OrderId nextId_{0};

  std::array<MatchResult, models::MAX_MATCH_EVENTS> matchResults_{};

  utils::MemoryPool<models::PriceLevel> priceLevelAllocator_{
      models::MAX_PRICE_LEVELS};
  models::DefaultOrderQueue::Allocator orderQueueAllocator_{10000};

  models::InstrumentId instrument_{};
};
} // namespace stockex::engine
