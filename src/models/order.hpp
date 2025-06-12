#pragma once

#include <array>
#include <format>
#include <vector>

#include "basic_types.hpp"
#include "constants.hpp"

namespace stockex::models {
struct Order {
  InstrumentId instrumentId_{INVALID_INSTRUMENT_ID};
  ClientId clientId_{INVALID_CLIENT_ID};
  OrderId clientOrderId_{INVALID_ORDER_ID};
  OrderId marketOrderId_{INVALID_ORDER_ID};
  Side side_{Side::INVALID};
  Price price_{INVALID_PRICE};
  Quantity qty_{INVALID_QUANTITY};
  Priority priority_{INVALID_PRIORITY};

  Order *prev_{};
  Order *next_{};

  constexpr Order() noexcept = default;

  constexpr Order(InstrumentId instrumentId, ClientId clientId,
                  OrderId clientOrderId, OrderId marketOrderId, Side side,
                  Price price, Quantity qty, Priority prty) noexcept
      : instrumentId_{instrumentId}, clientId_{clientId},
        clientOrderId_{clientOrderId}, marketOrderId_{marketOrderId},
        side_{side}, price_{price}, qty_{qty}, priority_{prty} {
    prev_ = next_ = this;
  }

  auto toString() const noexcept {
    return std::format(
        "Order[instrument_id:{} market_order_id:{} client_id:{} "
        "client_order_id:{} side:{} price:{} qty:{} priority:{}]",
        instrumentIdToString(instrumentId_), orderIdToString(marketOrderId_),
        clientIdToString(clientId_), orderIdToString(clientOrderId_),
        sideToString(side_), priceToString(price_), quantityToString(qty_),
        priorityToString(priority_));
  }
};

struct PriceLevel {
  Side side_{Side::INVALID};
  Price price_{INVALID_PRICE};
  Order *headOrder_{};
  PriceLevel *prev_{};
  PriceLevel *next_{};

  constexpr PriceLevel() noexcept = default;

  constexpr PriceLevel(Side side, Price price, Order *headOrder_) noexcept
      : side_{side}, price_{price}, headOrder_{headOrder_} {
    prev_ = next_ = this;
  }

  auto isMatchable(Price p) const noexcept {
    return side_ == Side::BUY ? price_ >= p : price_ <= p;
  }

  auto isBetter(const PriceLevel* p) const noexcept {
    return side_ == Side::BUY ? price_ > p->price_ : price_ < p->price_;
  }

  auto toString() const noexcept {
    return std::format(
        "PriceLevel[side:{} price:{} first_me_order:{} prev:{} next:{}]",
        sideToString(side_), priceToString(price_),
        headOrder_ ? headOrder_->toString() : "null",
        priceToString(prev_ ? prev_->price_ : INVALID_PRICE),
        priceToString(next_ ? next_->price_ : INVALID_PRICE));
  }
};

using OrderMap = std::vector<Order *>;
using ClientOrderMap = std::array<OrderMap, MAX_NUM_CLIENTS>;
using PriceLevelMap = std::array<PriceLevel *, MAX_PRICE_LEVELS>;
} // namespace stockex::models