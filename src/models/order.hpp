#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <vector>

#include "basic_types.hpp"
#include "constants.hpp"

namespace stockex::models {
using QueueSize = uint32_t;
struct QueuePosition {
  QueueSize logicalIndex_{};
  QueueSize offsetAtInsert_{};
};

struct OrderInfo {
  models::QueuePosition position_{};
  models::OrderId marketOrderId_{};
  models::Price price_{};
};

struct Order {
  QueuePosition position_{};
  OrderId clientOrderId_{INVALID_ORDER_ID};
  OrderId marketOrderId_{INVALID_ORDER_ID};
  Price price_{INVALID_PRICE};
  Quantity qty_{INVALID_QUANTITY};
  InstrumentId instrumentId_{INVALID_INSTRUMENT_ID};
  ClientId clientId_{INVALID_CLIENT_ID};
  Side side_{Side::INVALID};

  constexpr Order() noexcept = default;

  constexpr Order(QueuePosition position, InstrumentId instrumentId,
                  ClientId clientId, OrderId clientOrderId,
                  OrderId marketOrderId, Side side, Price price,
                  Quantity qty) noexcept
      : position_{position}, clientOrderId_{clientOrderId},
        marketOrderId_{marketOrderId}, price_{price}, qty_{qty},
        instrumentId_{instrumentId}, clientId_{clientId}, side_{side} {}

  auto toString() const noexcept {
    return std::format(
        "Order[instrument_id:{} market_order_id:{} client_id:{} "
        "client_order_id:{} side:{} price:{} qty:{}]",
        instrumentIdToString(instrumentId_), orderIdToString(marketOrderId_),
        clientIdToString(clientId_), orderIdToString(clientOrderId_),
        sideToString(side_), priceToString(price_), quantityToString(qty_));
  }
};

struct BasicOrder {
  OrderId orderId_{};
  Quantity qty_{};
  ClientId clientId_{};
  bool deleted_{false};
};

class OrderQueue {
private:
  auto compact() noexcept {
    if (head_ < COMPACT_THRESHOLD || head_ * 2 < orders_.size())
      return;

    auto remaining = orders_.size() - head_;
    std::memmove(&orders_[0], &orders_[head_], remaining * sizeof(BasicOrder));
    offset += head_;
    tail_ = static_cast<QueueSize>(remaining);
    head_ = 0;
  }

public:
  constexpr explicit OrderQueue(QueueSize size) { orders_.reserve(size); }

  auto push(BasicOrder order) noexcept -> QueuePosition {
    QueueSize index{};
    if (tail_ < orders_.size()) {
      orders_[tail_] = order;
      index = tail_;
      ++tail_;
    } else {
      orders_.push_back(order);
      index = static_cast<QueueSize>(orders_.size() - 1);
      ++tail_;
    }
    size_++;
    return {index, offset};
  };

  auto front() noexcept -> BasicOrder * {
    // compact();
    while (head_ != orders_.size() && orders_[head_].deleted_) {
      head_++;
    }
    return orders_[head_].deleted_ ? nullptr : &orders_[head_];
  }

  auto remove(QueuePosition pos) noexcept {
    auto [index, insert_offset] = pos;
    if (insert_offset == offset) {
      orders_[index].deleted_ = true;
    } else {
      orders_[index - offset].deleted_ = true;
    }
    size_--;
  }

  auto pop() noexcept {
    orders_[head_].deleted_ = true;
    head_++;
    size_--;
  }

  auto front() const noexcept -> BasicOrder { return orders_[head_]; }

  auto last() const noexcept -> BasicOrder { return orders_[tail_ - 1]; }

  auto empty() const noexcept -> bool { return size_ == 0; }

private:
  std::vector<BasicOrder> orders_{};
  QueueSize tail_{};
  QueueSize head_{};
  QueueSize offset{};
  QueueSize size_{};
  static const QueueSize COMPACT_THRESHOLD = 1000;
};

struct PriceLevel {
  Side side_{Side::INVALID};
  Price price_{INVALID_PRICE};
  OrderQueue orders{5000000};
  PriceLevel *prev_{};
  PriceLevel *next_{};

  constexpr PriceLevel() noexcept = default;

  constexpr PriceLevel(Side side, Price price) noexcept
      : side_{side}, price_{price} {
    prev_ = next_ = this;
  }

  auto isMatchable(Price p) const noexcept {
    return side_ == Side::BUY ? price_ >= p : price_ <= p;
  }

  auto isBetter(const PriceLevel *p) const noexcept {
    return side_ == Side::BUY ? price_ > p->price_ : price_ < p->price_;
  }

  auto toString() const noexcept {
    return std::format("PriceLevel[side:{} price:{} prev:{} next:{}]",
                       sideToString(side_), priceToString(price_),
                       priceToString(prev_ ? prev_->price_ : INVALID_PRICE),
                       priceToString(next_ ? next_->price_ : INVALID_PRICE));
  }
};

using OrderMap = std::vector<OrderInfo>;
using ClientOrderMap = std::array<OrderMap, MAX_NUM_CLIENTS>;
using PriceLevelMap = std::array<PriceLevel *, MAX_PRICE_LEVELS>;
} // namespace stockex::models