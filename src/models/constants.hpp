#pragma once

#include <cstddef>

namespace stockex::models {
/// Maximum instrustruments traded on the exchange.
constexpr std::size_t MAX_NUM_INSTRUMENTS = 8;

/// Maximum number of client requests and responses
constexpr std::size_t MAX_CLIENT_UPDATES = 256 * 1024;

/// Maximum number of market updates between components.
constexpr std::size_t MAX_MARKET_UPDATES = 256 * 1024;

/// Maximum trading clients.
constexpr std::size_t MAX_NUM_CLIENTS = 10;

/// Maximum number of orders
constexpr std::size_t MAX_NUM_ORDERS = 1000000;

/// Maximum price level depth in the order books.
constexpr std::size_t MAX_PRICE_LEVELS = 256;

constexpr std::size_t MAX_MATCH_EVENTS = 100;
} // namespace stockex::models