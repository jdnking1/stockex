#pragma once

#include <cstdint>
#include <format>

#include "basic_types.hpp"

namespace stockex::models {
#pragma pack(push, 1)
enum class ResponseType : uint8_t {
  INVALID = 0,
  ACCEPTED = 1,
  CANCELED = 2,
  MODIFIED = 3,
  FILLED = 4,
  CANCEL_REJECTED = 5,
  MODIFY_REJECTED = 6,
  INVALID_REQUEST = 7,
};

inline auto resTypeToString(ResponseType type) {
  using enum ResponseType;
  switch (type) {
  case ACCEPTED:
    return "ACCEPTED";
  case CANCELED:
    return "CANCELED";
  case FILLED:
    return "FILLED";
  case CANCEL_REJECTED:
    return "CANCEL_REJECTED";
  case MODIFIED:
    return "MODIFIED";
  case MODIFY_REJECTED:
    return "MODIFY_REJECTED";
  case INVALID_REQUEST:
    return "INVALID_REQUEST";
  case INVALID:
    return "INVALID";
  }
  return "UNKNOWN";
}

struct Response {
  ResponseType type_ = ResponseType::INVALID;
  ClientId clientId_ = INVALID_CLIENT_ID;
  InstrumentId instrumentId_ = INVALID_INSTRUMENT_ID;
  OrderId clientOrderId_ = INVALID_ORDER_ID;
  OrderId marketOrderId_ = INVALID_ORDER_ID;
  Side side_ = Side::INVALID;
  Price price_ = INVALID_PRICE;
  Quantity execQty = INVALID_QUANTITY;
  Quantity leavesQty_ = INVALID_QUANTITY;

  auto toString() const -> std::string {
    return std::format("Response[type:{} client:{} instrument:{} coid:{} "
                       "moid:{} side:{} exec qty:{} leaves qty:{} price:{}]",
                       resTypeToString(type_), clientIdToString(clientId_),
                       instrumentIdToString(instrumentId_),
                       orderIdToString(clientOrderId_),
                       orderIdToString(marketOrderId_), sideToString(side_),
                       quantityToString(execQty), quantityToString(leavesQty_),
                       priceToString(price_));
  }
};

struct SequencedResponse {
  uint64_t sequenceNumber_ = 0;
  Response response_;

  auto toString() const {
    return std::format("SequencedResponse[sequence number:{} response:{}]",
                       sequenceNumber_, response_.toString());
  }
};

#pragma pack(pop)
} // namespace stockex::models
