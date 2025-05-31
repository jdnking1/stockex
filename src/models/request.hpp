
#pragma once

#include <cstdint>
#include <format>

#include "basic_types.hpp"

namespace stockex::models {
#pragma pack(push, 1)
enum class RequestType : uint8_t {
  INVALID = 0,
  NEW = 1,
  CANCEL = 2,
  MODIFY = 3
};

inline auto reqTypeToString(RequestType type) {
  using enum RequestType;
  switch (type) {
  case NEW:
    return "NEW";
  case CANCEL:
    return "CANCEL";
  case MODIFY:
    return "MODIFY";
  case INVALID:
    return "INVALID";
  }
  return "UNKNOWN";
}

struct Request {
  RequestType type_ = RequestType::INVALID;
  ClientId clientId_ = INVALID_CLIENT_ID;
  InstrumentId instrumentId_ = INVALID_INSTRUMENT_ID;
  OrderId orderId_ = INVALID_ORDER_ID;
  Side side_ = Side::INVALID;
  Price price_ = INVALID_PRICE;
  Quantity qty_ = INVALID_QUANTITY;

  auto toString() const {
    return std::format("Request [type:{} client:{} "
                       "instrument:{} oid:{} side:{} qty:{} price:{}]",
                       reqTypeToString(type_), clientIdToString(clientId_),
                       instrumentIdToString(instrumentId_),
                       orderIdToString(orderId_), sideToString(side_),
                       quantityToString(qty_), priceToString(price_));
  }
};

struct SequencedRequest {
  uint64_t sequenceNumber_ = 0;
  Request request_;

  auto toString() const {
    return std::format("SequencedRequest[sequence number:{} request:{}]",
                       sequenceNumber_, request_.toString());
  }
};
#pragma pack(pop)
} // namespace stockex::models
