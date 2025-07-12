#pragma once

#include <array>
#include <format>
#include <vector>

#include "basic_types.hpp"
#include "constants.hpp"
#include "order_queue.hpp"

namespace stockex::models {

using DefaultOrderQueue = OrderQueue<>;
using QueueHandle = typename DefaultOrderQueue::Handle;

struct PriceLevel {
  Side side_{Side::INVALID};
  Price price_{INVALID_PRICE};
  DefaultOrderQueue orders_;
  PriceLevel *prev_{};
  PriceLevel *next_{};

  PriceLevel(Side side, Price price,
             DefaultOrderQueue::Allocator &allocator) noexcept
      : side_{side}, price_{price}, orders_{allocator} {
    prev_ = next_ = this;
  }

  [[nodiscard]] auto addOrder(const BasicOrder &order) noexcept -> QueueHandle {
    return orders_.push(order);
  }

  [[nodiscard]] auto getFrontOrder() noexcept -> BasicOrder * {
    return orders_.front();
  }

  auto removeOrder(DefaultOrderQueue::Handle handle) noexcept -> void {
    orders_.remove(handle);
  }

  auto popFrontOrder() noexcept -> void { orders_.pop(); }

  [[nodiscard]] auto isEmpty() const noexcept -> bool {
    return orders_.empty();
  }

  [[nodiscard]] auto isMatchable(Price p) const noexcept {
    return side_ == Side::BUY ? price_ >= p : price_ <= p;
  }

  [[nodiscard]] auto isBetter(const PriceLevel *p) const noexcept {
    return side_ == Side::BUY ? price_ > p->price_ : price_ < p->price_;
  }

  [[nodiscard]] auto toString() const noexcept {
    return std::format("PriceLevel[side:{} price:{} prev:{} next:{}]",
                       sideToString(side_), priceToString(price_),
                       priceToString(prev_ ? prev_->price_ : INVALID_PRICE),
                       priceToString(next_ ? next_->price_ : INVALID_PRICE));
  }
};

struct OrderInfo {
  QueueHandle queueHandle_{};
  models::OrderId marketOrderId_{};
  models::Price price_{};
};

using OrderMap = std::vector<OrderInfo>;
using ClientOrderMap = std::array<OrderMap, MAX_NUM_CLIENTS>;
using PriceLevelMap = std::array<PriceLevel *, MAX_PRICE_LEVELS>;
} // namespace stockex::models