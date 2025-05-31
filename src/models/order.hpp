#pragma once

#include <array>
#include <format>
#include <vector>

#include "basic_types.hpp"
#include "constants.hpp"

namespace stockex::models {
struct Order {
  InstrumentId instrumentId_ = INVALID_INSTRUMENT_ID;
  ClientId clientId_ = INVALID_CLIENT_ID;
  OrderId clientOrderId_ = INVALID_ORDER_ID;
  OrderId marketOrderId_ = INVALID_ORDER_ID;
  Side side_ = Side::INVALID;
  Price price_ = INVALID_PRICE;
  Quantity qty_ = INVALID_QUANTITY;
  Priority priority_ = INVALID_PRIORITY;

  Order *prev_ = nullptr;
  Order *next_ = nullptr;

  constexpr Order() = default;

  constexpr Order(InstrumentId instrumentId, ClientId clientId,
                  OrderId clientOrderId, OrderId marketOrderId, Side side,
                  Price price, Quantity qty, Priority prty) noexcept
      : instrumentId_{instrumentId}, clientId_{clientId},
        clientOrderId_{clientOrderId}, marketOrderId_{marketOrderId},
        side_{side}, price_{price}, qty_{qty}, priority_{prty} {}

  auto toString() const {
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
  Side side_ = Side::INVALID;
  Price price_ = INVALID_PRICE;
  Order *firstOrder_ = nullptr;
  PriceLevel *prev_ = nullptr;
  PriceLevel *next_ = nullptr;

  constexpr PriceLevel() = default;

  constexpr PriceLevel(Side side, Price price, Order *first_order,
                       PriceLevel *prev_entry, PriceLevel *next_entry)
      : side_(side), price_(price), firstOrder_(first_order), prev_(prev_entry),
        next_(next_entry) {}

  auto toString() const {
    return std::format(
        "price_level[side:{} price:{} first_me_order:{} prev:{} next:{}]",
        sideToString(side_), priceToString(price_),
        firstOrder_ ? firstOrder_->toString() : "null",
        priceToString(prev_ ? prev_->price_ : INVALID_PRICE),
        priceToString(next_ ? next_->price_ : INVALID_PRICE));
  }
};

using OrderMap = std::vector<Order *>;
using ClientOrderMap = std::array<OrderMap, MAX_NUM_CLIENTS>;
using PriceLevelMap = std::array<PriceLevel *, MAX_PRICE_LEVELS>;
} // namespace stockex::models