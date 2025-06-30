#include "order_book.hpp"

#include <cstddef>

#include "models/basic_types.hpp"
#include "models/constants.hpp"
#include "models/order.hpp"

namespace stockex::engine {
auto OrderBook::addOrder(models::ClientId clientId,
                         models::OrderId clientOrderId,
                         models::OrderId marketOrderId, models::Side side,
                         models::Price price,
                         models::Quantity quantity) noexcept -> void {
  auto priceLevel = getPriceLevel(price);
  models::Order *order = nullptr;
  if (!priceLevel) {
    order = orderAllocator_.alloc(models::QueuePosition{}, instrument_,
                                  clientId, clientOrderId, marketOrderId, side,
                                  price, quantity);
    addPriceLevel(order);
  } else {
    auto position =
        priceLevel->orders.push({clientOrderId, quantity, clientId});
    order =
        orderAllocator_.alloc(position, instrument_, clientId, clientOrderId,
                              marketOrderId, side, price, quantity);
  }
  clientOrders_[clientId][clientOrderId] = order;
}

auto OrderBook::removeOrder(models::ClientId clientId,
                            models::OrderId orderId) noexcept -> void {
  auto order = clientOrders_[clientId][orderId];
  auto priceLevel = getPriceLevel(order->price_);
  priceLevel->orders.remove(order->position_);
  clientOrders_[clientId][orderId] = nullptr;
  orderAllocator_.free(order);
  if (priceLevel->orders.empty()) {
    removePriceLevel(order->side_, order->price_);
  }
}

auto OrderBook::addPriceLevel(models::Order *order) noexcept -> void {
  auto priceLevel = priceLevelAllocator_.alloc(order->side_, order->price_);
  auto position = priceLevel->orders.push(
      {order->clientOrderId_, order->qty_, order->clientId_});
  order->position_ = position;
  auto priceLevelIndex = getPriceIndex(priceLevel->price_);
  priceLevels_[priceLevelIndex] = priceLevel;
  auto *&bestPriceLevel =
      priceLevel->side_ == models::Side::BUY ? bestBid_ : bestAsk_;
  const auto pushAfter = [](models::PriceLevel *current,
                            models::PriceLevel *newPriceLevel) noexcept {
    newPriceLevel->next_ = current->next_;
    newPriceLevel->prev_ = current;
    current->next_->prev_ = newPriceLevel;
    current->next_ = newPriceLevel;
  };
  const auto pushBefore = [](models::PriceLevel *current,
                             models::PriceLevel *newPriceLevel) noexcept {
    newPriceLevel->prev_ = current->prev_;
    newPriceLevel->next_ = current;
    current->prev_->next_ = newPriceLevel;
    current->prev_ = newPriceLevel;
  };
  if (!bestPriceLevel) {
    bestPriceLevel = priceLevel;
  } else if (priceLevel->isBetter(bestPriceLevel)) {
    pushBefore(bestPriceLevel, priceLevel);
    bestPriceLevel = priceLevel;
  } else {
    auto addAtTheEnd = false;
    auto currentPriceLevel = bestPriceLevel->next_;
    while (!addAtTheEnd && !priceLevel->isBetter(currentPriceLevel)) {
      currentPriceLevel = currentPriceLevel->next_;
      if (currentPriceLevel == bestPriceLevel) {
        addAtTheEnd = true;
      }
    }
    if (addAtTheEnd) {
      pushAfter(bestPriceLevel->prev_, priceLevel);
    } else {
      pushBefore(currentPriceLevel, priceLevel);
    }
  }
}

auto OrderBook::removePriceLevel(models::Side side,
                                 models::Price price) noexcept -> void {
  auto priceLevel = getPriceLevel(price);
  auto *&bestPriceLevel = side == models::Side::BUY ? bestBid_ : bestAsk_;
  if (priceLevel->next_ == priceLevel) {
    bestPriceLevel = nullptr;
  } else {
    priceLevel->prev_->next_ = priceLevel->next_;
    priceLevel->next_->prev_ = priceLevel->prev_;
    if (bestPriceLevel->price_ == priceLevel->price_) {
      bestPriceLevel = priceLevel->next_;
    }
    priceLevel->next_ = priceLevel->prev_ = nullptr;
  }
  auto priceLevelIndex = getPriceIndex(price);
  priceLevels_[priceLevelIndex] = nullptr;
  priceLevelAllocator_.free(priceLevel);
}

auto OrderBook::match(models::ClientId clientId, models::OrderId orderId,
                      models::Side side, models::Price price,
                      models::Quantity quantity) noexcept -> MatchResultSet {
  auto &bestPriceLevel = side == models::Side::BUY ? bestAsk_ : bestBid_;
  auto remainingQuantity = quantity;
  std::size_t matchCount{};
  auto overflow{false};
  while (remainingQuantity && bestPriceLevel &&
         bestPriceLevel->isMatchable(price) &&
         matchCount < models::MAX_MATCH_EVENTS) {
    auto matchedOrder = bestPriceLevel->orders.front();
    auto matchedQty = std::min(remainingQuantity, matchedOrder->qty);
    remainingQuantity -= matchedQty;
    matchedOrder->qty -= matchedQty;
    matchResults_[matchCount] = {orderId,
                                 matchedOrder->orderId_,
                                 bestPriceLevel->price_,
                                 matchedQty,
                                 matchedOrder->qty,
                                 clientId,
                                 matchedOrder->clientId_,
                                 side,
                                 bestPriceLevel->side_};

    if (matchedOrder->qty == 0) {
      removeOrder(matchedOrder->clientId_, matchedOrder->orderId_);
    }
    matchCount++;
  }
  if (matchCount == models::MAX_MATCH_EVENTS && bestPriceLevel &&
      bestPriceLevel->isMatchable(price)) [[unlikely]] {
    overflow = true;
  }
  return {{matchResults_.data(), matchCount},
          remainingQuantity,
          instrument_,
          overflow};
}

} // namespace stockex::engine