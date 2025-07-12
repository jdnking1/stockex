#include "order_book.hpp"

namespace stockex::engine {
auto OrderBook::addOrder(models::ClientId clientId,
                         models::OrderId clientOrderId,
                         models::OrderId marketOrderId, models::Side side,
                         models::Price price,
                         models::Quantity quantity) noexcept -> void {
  auto *priceLevel = getPriceLevel(price);

  if (!priceLevel) {
    priceLevel = addPriceLevel(side, price);
  }

  auto queueHandle = priceLevel->addOrder({clientOrderId, quantity, clientId});
  clientOrders_[clientId][clientOrderId] = {queueHandle, marketOrderId, price};
}

auto OrderBook::removeOrder(models::ClientId clientId,
                            models::OrderId orderId) noexcept -> void {
  const auto &order = clientOrders_[clientId][orderId];
  auto *priceLevel = getPriceLevel(order.price_);
  if (priceLevel) [[likely]] {
    priceLevel->removeOrder(order.queueHandle_);
    if (priceLevel->isEmpty()) {
      removePriceLevel(priceLevel);
    }
  }
}

auto OrderBook::match(models::ClientId clientId, models::OrderId orderId,
                      models::Side side, models::Price price,
                      models::Quantity quantity) noexcept -> MatchResultSet {
  auto *&bestPriceLevel = (side == models::Side::BUY) ? bestAsk_ : bestBid_;
  auto remainingQuantity = quantity;
  std::size_t matchCount{};

  while (remainingQuantity > 0 && bestPriceLevel &&
         bestPriceLevel->isMatchable(price) &&
         matchCount < models::MAX_MATCH_EVENTS) {

    auto *matchedOrder = bestPriceLevel->getFrontOrder();
    const auto matchedQty = std::min(remainingQuantity, matchedOrder->qty_);

    remainingQuantity -= matchedQty;
    matchedOrder->qty_ -= matchedQty;

    // Record the trade event.
    matchResults_[matchCount] = {orderId,
                                 matchedOrder->orderId_,
                                 bestPriceLevel->price_,
                                 matchedQty,
                                 matchedOrder->qty_,
                                 clientId,
                                 matchedOrder->clientId_,
                                 side,
                                 bestPriceLevel->side_};
    matchCount++;

    if (matchedOrder->qty_ == 0) {
      removeHeadOrder(bestPriceLevel);
    }
  }

  const bool overflow = (matchCount == models::MAX_MATCH_EVENTS &&
                         bestPriceLevel && bestPriceLevel->isMatchable(price));

  return {{matchResults_.data(), matchCount},
          remainingQuantity,
          instrument_,
          overflow};
}

auto OrderBook::insertPriceLevelBefore(
    models::PriceLevel *current, models::PriceLevel *newPriceLevel) noexcept
    -> void {
  newPriceLevel->prev_ = current->prev_;
  newPriceLevel->next_ = current;
  current->prev_->next_ = newPriceLevel;
  current->prev_ = newPriceLevel;
}

auto OrderBook::addPriceLevel(models::Side side, models::Price price) noexcept
    -> models::PriceLevel * {
  auto *&bestPriceLevel = (side == models::Side::BUY) ? bestBid_ : bestAsk_;

  auto *newPriceLevel =
      priceLevelAllocator_.alloc(side, price, OrderQueueAllocator);
  priceLevels_[getPriceIndex(price)] = newPriceLevel;

  if (!bestPriceLevel) {
    bestPriceLevel = newPriceLevel;
    return newPriceLevel;
  }

  if (newPriceLevel->isBetter(bestPriceLevel)) {
    insertPriceLevelBefore(bestPriceLevel, newPriceLevel);
    bestPriceLevel = newPriceLevel;
    return newPriceLevel;
  }

  auto *current = bestPriceLevel->next_;
  while (current != bestPriceLevel && !newPriceLevel->isBetter(current)) {
    current = current->next_;
  }
  insertPriceLevelBefore(current, newPriceLevel);

  return newPriceLevel;
}

auto OrderBook::removePriceLevel(models::PriceLevel *priceLevel) noexcept
    -> void {
  auto *&bestPriceLevel =
      (priceLevel->side_ == models::Side::BUY) ? bestBid_ : bestAsk_;

  if (priceLevel->next_ == priceLevel) {
    bestPriceLevel = nullptr;
  } else {
    priceLevel->prev_->next_ = priceLevel->next_;
    priceLevel->next_->prev_ = priceLevel->prev_;

    if (bestPriceLevel == priceLevel) {
      bestPriceLevel = priceLevel->next_;
    }
    priceLevel->next_ = priceLevel->prev_ = nullptr;
  }
  priceLevels_[getPriceIndex(priceLevel->price_)] = nullptr;
  priceLevelAllocator_.free(priceLevel);
}
} // namespace stockex::engine