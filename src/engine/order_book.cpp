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
  models::QueuePosition position{};

  if (!priceLevel) {
    priceLevel = addPriceLevel(side, price);
  }

  position = priceLevel->orders.push({clientOrderId, quantity, clientId});
  clientOrders_[clientId][clientOrderId] = {position, marketOrderId, price};
}

auto OrderBook::removeOrder(models::ClientId clientId,
                            models::OrderId orderId) noexcept -> void {
  auto order = clientOrders_[clientId][orderId];
  auto priceLevel = getPriceLevel(order.price_);
  priceLevel->orders.remove(order.position_);
  if (priceLevel->orders.empty()) {
    removePriceLevel(priceLevel);
  }
}

auto OrderBook::addPriceLevel(models::Side side, models::Price price) noexcept
    -> models::PriceLevel * {
  auto priceLevel = priceLevelAllocator_.alloc(side, price);
  auto priceLevelIndex = getPriceIndex(price);
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

  return priceLevel;
}

auto OrderBook::removePriceLevel(models::PriceLevel *priceLevel) noexcept
    -> void {
  auto *&bestPriceLevel =
      priceLevel->side_ == models::Side::BUY ? bestBid_ : bestAsk_;
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
  auto priceLevelIndex = getPriceIndex(priceLevel->price_);
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
    auto matchedQty = std::min(remainingQuantity, matchedOrder->qty_);
    remainingQuantity -= matchedQty;
    matchedOrder->qty_ -= matchedQty;
    matchResults_[matchCount] = {orderId,
                                 matchedOrder->orderId_,
                                 bestPriceLevel->price_,
                                 matchedQty,
                                 matchedOrder->qty_,
                                 clientId,
                                 matchedOrder->clientId_,
                                 side,
                                 bestPriceLevel->side_};

    if (matchedOrder->qty_ == 0) {
      removeHeadOrder(bestPriceLevel);
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