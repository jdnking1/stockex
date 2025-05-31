#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace stockex::models {
using OrderId = uint64_t;
constexpr auto INVALID_ORDER_ID = std::numeric_limits<OrderId>::max();
inline auto orderIdToString(OrderId id) -> std::string {
  if (id == INVALID_ORDER_ID) {
    return "INVALID";
  }

  return std::to_string(id);
}

using InstrumentId = uint8_t;
constexpr auto INVALID_INSTRUMENT_ID = std::numeric_limits<InstrumentId>::max();
inline auto instrumentIdToString(InstrumentId id) -> std::string {
  if (id == INVALID_INSTRUMENT_ID) {
    return "INVALID";
  }

  return std::to_string(id);
}

using ClientId = uint32_t;
constexpr auto INVALID_CLIENT_ID = std::numeric_limits<ClientId>::max();
inline auto clientIdToString(ClientId id) -> std::string {
  if (id == INVALID_CLIENT_ID) {
    return "INVALID";
  }

  return std::to_string(id);
}

using Price = int64_t;
constexpr auto INVALID_PRICE = std::numeric_limits<Price>::max();
inline auto priceToString(Price price) -> std::string {
  if (price == INVALID_PRICE) {
    return "INVALID";
  }

  return std::to_string(price);
}

using Quantity = uint32_t;
constexpr auto INVALID_QUANTITY = std::numeric_limits<Quantity>::max();
inline auto quantityToString(Quantity quantity) -> std::string {
  if (quantity == INVALID_QUANTITY) {
    return "INVALID";
  }

  return std::to_string(quantity);
}

using Priority = uint64_t;
constexpr auto INVALID_PRIORITY = std::numeric_limits<Priority>::max();
inline auto priorityToString(Priority priority) -> std::string {
  if (priority == INVALID_PRIORITY) {
    return "INVALID";
  }

  return std::to_string(priority);
}

enum class Side : uint8_t { INVALID = 0, BUY = 1, SELL = 2 };

inline auto sideToString(Side side) -> std::string {
  using enum Side;
  switch (side) {
  case BUY:
    return "BUY";
  case SELL:
    return "SELL";
  case INVALID:
    return "INVALID";
  }

  return "UNKNOWN";
}
} // namespace stockex::models