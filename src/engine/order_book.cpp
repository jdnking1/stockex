#include "order_book.hpp"

#include <cstddef>

#include "models/basic_types.hpp"
#include "models/constants.hpp"

namespace stockex::engine {
auto OrderBook::addOrder(models::ClientId clientId,
                         models::OrderId clientOrderId,
                         models::OrderId marketOrderId, models::Side side,
                         models::Price price,
                         models::Quantity quantity) noexcept -> void {
  auto priceLevel = getPriceLevel(price);
  auto order = orderAllocator_.alloc(instrument_, clientId, clientOrderId,
                                     marketOrderId, side, price, quantity);
  if (!priceLevel) {
    addPriceLevel(order);
  } else {
    auto *tailOrder = priceLevel->headOrder_->prev_;
    tailOrder->next_ = order;
    order->prev_ = tailOrder;
    order->next_ = priceLevel->headOrder_;
    priceLevel->headOrder_->prev_ = order;
  }
  clientOrders_[clientId][clientOrderId] = order;
}

auto OrderBook::removeOrder(models::ClientId clientId,
                            models::OrderId orderId) noexcept -> void {
  auto order = clientOrders_[clientId][orderId];
  auto priceLevel = getPriceLevel(order->price_);
  if (order->prev_ == order) {
    removePriceLevel(order->side_, order->price_);
  } else {
    auto prevOrder = order->prev_;
    auto nextOrder = order->next_;
    prevOrder->next_ = nextOrder;
    nextOrder->prev_ = prevOrder;
    if (priceLevel->headOrder_ == order) {
      priceLevel->headOrder_ = nextOrder;
    }
    order->prev_ = order->next_ = nullptr;
  }
  clientOrders_[clientId][orderId] = nullptr;
  orderAllocator_.free(order);
}

auto OrderBook::addPriceLevel(models::Order *order) noexcept -> void {
  auto priceLevel =
      priceLevelAllocator_.alloc(order->side_, order->price_, order);
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
    auto matchedOrder = bestPriceLevel->headOrder_;
    auto matchedQty = std::min(remainingQuantity, matchedOrder->qty_);
    remainingQuantity -= matchedQty;
    matchedOrder->qty_ -= matchedQty;
    matchResults_[matchCount] = {orderId,
                                 matchedOrder->clientOrderId_,
                                 matchedOrder->price_,
                                 matchedQty,
                                 matchedOrder->qty_,
                                 clientId,
                                 matchedOrder->clientId_,
                                 side,
                                 matchedOrder->side_};

    if (matchedOrder->qty_ == 0) {
      removeOrder(matchedOrder->clientId_, matchedOrder->clientOrderId_);
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