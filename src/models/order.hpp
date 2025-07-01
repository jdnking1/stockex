#pragma once

#include <array>
#include <format>
#include <vector>

#include "basic_types.hpp"
#include "constants.hpp"
#include "order_queue.hpp"

namespace stockex::models {

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