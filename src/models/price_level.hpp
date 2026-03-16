#pragma once

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

  PriceLevel(Side side, Price price,
             DefaultOrderQueue::Allocator &allocator) noexcept
      : side_{side}, price_{price}, orders_{allocator} {}

  [[nodiscard]] auto addOrder(const BasicOrder &order) noexcept -> QueueHandle {
    return orders_.push(order);
  }

  [[nodiscard]] auto getFrontOrder() noexcept -> BasicOrder * {
    return orders_.front();
  }

  [[nodiscard]] auto removeOrder(DefaultOrderQueue::Handle handle) noexcept
      -> bool {
    return orders_.remove(handle);
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
    return std::format("PriceLevel[side:{} price:{}]", sideToString(side_),
                       priceToString(price_));
  }
};

struct OrderInfo {
  QueueHandle queueHandle_{};
  models::Price price_{models::INVALID_PRICE};
};
using PriceLevelVec = std::vector<PriceLevel *>;
} // namespace stockex::models