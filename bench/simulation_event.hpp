#pragma once

#include "models/basic_types.hpp"

namespace stockex::benchmarks {
enum class EventType : uint8_t { ADD, CANCEL, MATCH, PREFILL };

struct SimulationEvent {
  models::OrderId orderId{};
  models::Price price{};
  models::Quantity qty{};
  models::Side side{};
  EventType type{};
  models::ClientId clientId{};
  char padding[5]{};
};
} // namespace stockex::benchmarks