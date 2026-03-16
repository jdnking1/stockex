#include "order_book.hpp"

#include <algorithm>

namespace stockex::engine {
auto OrderBook::addOrder(models::ClientId clientId, models::Side side,
                         models::Price price, models::Quantity quantity) noexcept
    -> std::expected<models::OrderId, OrderBookError> {
  if (freeList_.empty() && nextId_ >= orders_.size()) [[unlikely]]
    return std::unexpected(OrderBookError::OrderIdExhausted);

  auto *priceLevel = getPriceLevel(price);

  if (!priceLevel) {
    priceLevel = addPriceLevel(side, price);
    if (!priceLevel) [[unlikely]]
      return std::unexpected(OrderBookError::OrderQueuePoolExhausted);
  }

  auto orderId = allocateOrderId();
  auto queueHandle = priceLevel->addOrder({orderId, quantity, clientId});
  if (!queueHandle.chunk_) [[unlikely]] {
    releaseOrderId(orderId);
    return std::unexpected(OrderBookError::OrderQueuePoolExhausted);
  }
  orders_[orderId] = {queueHandle, price};
  return orderId;
}

auto OrderBook::removeOrder(models::OrderId orderId) noexcept
    -> std::expected<void, OrderBookError> {
  using enum OrderBookError;
  if (orderId >= orders_.size()) [[unlikely]]
    return std::unexpected(InvalidOrderId);
  auto &order = orders_[orderId];
  if (order.price_ == models::INVALID_PRICE) [[unlikely]]
    return std::unexpected(InvalidOrderId);
  auto *priceLevel = getPriceLevel(order.price_);
  if (!priceLevel) [[unlikely]] {
    order.price_ = models::INVALID_PRICE;
    return std::unexpected(InvalidOrderId);
  }
  if (!priceLevel->removeOrder(order.queueHandle_)) [[unlikely]] {
    order.price_ = models::INVALID_PRICE;
    return std::unexpected(InvalidOrderId);
  }
  order.price_ = models::INVALID_PRICE;
  if (priceLevel->isEmpty()) {
    removePriceLevel(priceLevel->price_, priceLevel->side_);
  }
  releaseOrderId(orderId);
  return {};
}

auto OrderBook::match(models::ClientId clientId, models::Side side,
                      models::Price price,
                      models::Quantity quantity) noexcept -> MatchResultSet {
  auto &levels = (side == models::Side::BUY) ? asks_ : bids_;
  auto incomingOrderId = peekNextOrderId();
  auto remainingQuantity = quantity;
  std::size_t matchCount{};

  while (remainingQuantity > 0 && !levels.empty() &&
         levels.front().isMatchable(price) &&
         matchCount < models::MAX_MATCH_EVENTS) {

    auto *matchedOrder = levels.front().getFrontOrder();
    const auto matchedQty = std::min(remainingQuantity, matchedOrder->qty_);

    remainingQuantity -= matchedQty;
    matchedOrder->qty_ -= matchedQty;

    matchResults_[matchCount] = {incomingOrderId,
                                 matchedOrder->orderId_,
                                 levels.front().price_,
                                 matchedQty,
                                 matchedOrder->qty_,
                                 clientId,
                                 matchedOrder->clientId_,
                                 side,
                                 levels.front().side_};
    matchCount++;

    if (matchedOrder->qty_ == 0) {
      removeHeadOrder(levels);
    }
  }

  const bool overflow =
      (matchCount == models::MAX_MATCH_EVENTS && !levels.empty() &&
       levels.front().isMatchable(price));

  return {{matchResults_.data(), matchCount},
          remainingQuantity,
          instrument_,
          overflow};
}

auto OrderBook::addPriceLevel(models::Side side, models::Price price) noexcept
    -> models::PriceLevel * {
  auto &levels = (side == models::Side::BUY) ? bids_ : asks_;

  auto it = std::find_if(
      levels.begin(), levels.end(),
      [side, price](const auto &pl) {
        return side == models::Side::BUY ? price > pl.price_
                                         : price < pl.price_;
      });
  auto inserted = levels.emplace(it, side, price, orderQueueAllocator_);
  return &*inserted;
}

auto OrderBook::removePriceLevel(models::Price price,
                                 models::Side side) noexcept -> void {
  auto &levels = (side == models::Side::BUY) ? bids_ : asks_;

  auto it = std::find_if(levels.begin(), levels.end(),
                         [price](const auto &pl) { return pl.price_ == price; });
  if (it != levels.end()) {
    levels.erase(it);
  }
}
} // namespace stockex::engine
