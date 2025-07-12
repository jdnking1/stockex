#include <algorithm>
#include <chrono>
#include <memory>

#include <gtest/gtest.h>
#include <random>

#include "engine/order_book.hpp"
#include "models/basic_types.hpp"
#include "models/order.hpp"

namespace stockex::engine {

using enum models::Side;
class OrderBookTest : public ::testing::Test {
protected:
  void SetUp() override { book_ = std::make_unique<OrderBook>(1); }

  auto AddOrderAndVerify(models::ClientId clientId,
                         models::OrderId clientOrderId,
                         models::OrderId marketOrderId, models::Side side,
                         models::Price price, models::Quantity qty) {
    book_->addOrder(clientId, clientOrderId, marketOrderId, side, price, qty);
    auto &orderInfo = book_->getOrder(clientId, clientOrderId);
    const auto *priceLevel = book_->getPriceLevel(price);
    auto bOrder = priceLevel->orders.last();
    EXPECT_EQ(bOrder->clientId_, clientId);
    EXPECT_EQ(bOrder->orderId_, clientOrderId);
    EXPECT_EQ(orderInfo.marketOrderId_, marketOrderId);
    EXPECT_EQ(priceLevel->side_, side);
    EXPECT_EQ(orderInfo.price_, price);
    EXPECT_EQ(bOrder->qty_, qty);
  }

  auto VerifyMatchResult(const MatchResult &result,
                         models::OrderId incomingOrderId,
                         models::OrderId matchedOrderId, models::Price price,
                         models::Quantity qty, models::Quantity remainingQty,
                         models::ClientId incomingClientId,
                         models::ClientId matchedClientId,
                         models::Side incomingSide,
                         models::Side matchedSide) const {
    EXPECT_EQ(result.incomingOrderId_, incomingOrderId);
    EXPECT_EQ(result.matchedOrderId_, matchedOrderId);
    EXPECT_EQ(result.price_, price);
    EXPECT_EQ(result.quantity_, qty);
    EXPECT_EQ(result.matchedOrderRemainingQty_, remainingQty);
    EXPECT_EQ(result.incomingClientId_, incomingClientId);
    EXPECT_EQ(result.matchedClientId_, matchedClientId);
    EXPECT_EQ(result.incomingOrderSide_, incomingSide);
    EXPECT_EQ(result.matchedOrderSide_, matchedSide);
  }

  auto getOrderBook() const -> const std::unique_ptr<OrderBook> & {
    return book_;
  }

private:
  std::unique_ptr<OrderBook> book_;
};

TEST_F(OrderBookTest, AddSingleBuyOrder) {
  AddOrderAndVerify(1, 100, 100, BUY, 100, 50);
}

TEST_F(OrderBookTest, AddSingleSellOrder) {
  AddOrderAndVerify(1, 101, 101, SELL, 200, 30);
}

TEST_F(OrderBookTest, AddMultipleOrdersSamePriceLevel) {
  AddOrderAndVerify(1, 100, 100, BUY, 100, 50);
  AddOrderAndVerify(1, 101, 101, BUY, 100, 30);
  const auto &order1 = getOrderBook()->getOrder(1, 100);
  const auto &order2 = getOrderBook()->getOrder(1, 101);
  EXPECT_EQ(order1.queueHandle_.index_, 0);
  EXPECT_EQ(order2.queueHandle_.index_, 1);
}

TEST_F(OrderBookTest, AddOrdersDifferentPriceLevels) {
  AddOrderAndVerify(1, 100, 100, BUY, 100, 50);
  AddOrderAndVerify(1, 101, 101, BUY, 101, 30);
  models::PriceLevel *bestBid = getOrderBook()->getPriceLevel(101);
  ASSERT_NE(bestBid, nullptr);
  EXPECT_EQ(bestBid->price_, 101);
  EXPECT_EQ(bestBid->next_->price_, 100);
}

TEST_F(OrderBookTest, RemoveOrder) {
  AddOrderAndVerify(1, 100, 100, BUY, 100, 50);
  getOrderBook()->removeOrder(1, 100);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, RemoveOrderFromMultiOrderPriceLevel) {
  AddOrderAndVerify(1, 100, 100, BUY, 100, 50);
  AddOrderAndVerify(1, 101, 101, BUY, 100, 30);
  getOrderBook()->removeOrder(1, 100);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100)->orders.front()->qty_, 30);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100)->orders.front()->orderId_, 101);
}

TEST_F(OrderBookTest, MatchSingleFullFill) {
  AddOrderAndVerify(1, 100, 100, SELL, 100, 50);
  auto result = getOrderBook()->match(2, 101, BUY, 100, 50);
  ASSERT_EQ(result.matches_.size(), 1);
  VerifyMatchResult(result.matches_[0], 101, 100, 100, 50, 0, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 0);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, MatchSinglePartialFillIncoming) {
  AddOrderAndVerify(1, 100, 100, SELL, 100, 30);
  auto result = getOrderBook()->match(2, 101, BUY, 100, 50);
  ASSERT_EQ(result.matches_.size(), 1);
  VerifyMatchResult(result.matches_[0], 101, 100, 100, 30, 0, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 20);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, MatchSinglePartialFillResting) {
  AddOrderAndVerify(1, 100, 100, SELL, 100, 50);
  auto result = getOrderBook()->match(2, 101, BUY, 100, 30);
  ASSERT_EQ(result.matches_.size(), 1);
  VerifyMatchResult(result.matches_[0], 101, 100, 100, 30, 20, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 0);
  auto *priceLevel = getOrderBook()->getPriceLevel(100);
  ASSERT_NE(priceLevel, nullptr);
  EXPECT_EQ(priceLevel->orders.front()->qty_, 20);
}

TEST_F(OrderBookTest, MatchMultipleOrdersSamePriceLevel) {
  AddOrderAndVerify(1, 100, 100, SELL, 100, 20);
  AddOrderAndVerify(1, 101, 101, SELL, 100, 20);
  auto result = getOrderBook()->match(2, 102, BUY, 100, 50);
  ASSERT_EQ(result.matches_.size(), 2);
  VerifyMatchResult(result.matches_[0], 102, 100, 100, 20, 0, 2, 1, BUY, SELL);
  VerifyMatchResult(result.matches_[1], 102, 101, 100, 20, 0, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 10);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, MatchMultiplePriceLevels) {
  AddOrderAndVerify(1, 100, 100, SELL, 100, 20);
  AddOrderAndVerify(1, 101, 101, SELL, 99, 20);
  auto result = getOrderBook()->match(2, 102, BUY, 100, 50);
  ASSERT_EQ(result.matches_.size(), 2);
  VerifyMatchResult(result.matches_[0], 102, 101, 99, 20, 0, 2, 1, BUY, SELL);
  VerifyMatchResult(result.matches_[1], 102, 100, 100, 20, 0, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 10);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
  EXPECT_EQ(getOrderBook()->getPriceLevel(99), nullptr);
}

TEST_F(OrderBookTest, NoMatchPriceMismatch) {
  AddOrderAndVerify(1, 100, 100, SELL, 101, 50);
  auto result = getOrderBook()->match(2, 101, BUY, 100, 50);
  EXPECT_EQ(result.matches_.size(), 0);
  EXPECT_EQ(result.remainingQuantity_, 50);
  auto *priceLevel = getOrderBook()->getPriceLevel(101);
  ASSERT_NE(priceLevel, nullptr);
  EXPECT_EQ(priceLevel->orders.front()->qty_, 50);
}

TEST_F(OrderBookTest, MatchMaxEventsLimit) {
  for (models::OrderId i = 100; i < 100 + models::MAX_MATCH_EVENTS + 1; ++i) {
    AddOrderAndVerify(1, i, i, SELL, 100, 10);
  }
  auto result = getOrderBook()->match(2, 200, BUY, 100, 10000);
  EXPECT_EQ(result.matches_.size(), models::MAX_MATCH_EVENTS);
  EXPECT_EQ(result.overflow_, true);
  EXPECT_EQ(result.remainingQuantity_, 10000 - models::MAX_MATCH_EVENTS * 10);
  EXPECT_NE(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, ComplexScenario) {
  AddOrderAndVerify(1, 100, 100, SELL, 100, 25);
  AddOrderAndVerify(1, 101, 101, SELL, 101, 30);
  AddOrderAndVerify(1, 102, 102, SELL, 99, 40);
  AddOrderAndVerify(2, 200, 200, BUY, 98, 50);
  AddOrderAndVerify(2, 201, 201, BUY, 97, 60);

  auto result = getOrderBook()->match(3, 300, BUY, 100, 100);
  ASSERT_EQ(result.matches_.size(), 2);
  VerifyMatchResult(result.matches_[0], 300, 102, 99, 40, 0, 3, 1, BUY, SELL);
  VerifyMatchResult(result.matches_[1], 300, 100, 100, 25, 0, 3, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 35);

  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
  EXPECT_EQ(getOrderBook()->getPriceLevel(99), nullptr);
  auto *priceLevel = getOrderBook()->getPriceLevel(101);
  ASSERT_NE(priceLevel, nullptr);
  EXPECT_EQ(priceLevel->orders.front()->qty_, 30);
}

#ifdef NDEBUG
TEST_F(OrderBookTest, PerformanceTestAddOrder) {
  const int numOrders = 500000;
  for (models::OrderId i = 0; i < numOrders; ++i) {
    getOrderBook()->addOrder(1, i, i, BUY, 100 + (i % 10), 50);
  }
  auto start = std::chrono::high_resolution_clock::now();
  for (models::OrderId i = numOrders; i < numOrders + numOrders; ++i) {
    getOrderBook()->addOrder(1, i, i, BUY, 100 + (i % 10), 50);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro> duration = end - start;
  EXPECT_TRUE(duration.count() / numOrders < 0.04);
}

TEST_F(OrderBookTest, PerformanceTestRemoveOrder) {
  const int numOrders = 500000;
  std::vector<int> orderIds(500000);
  orderIds.reserve(numOrders);

  for (models::OrderId i = 0; i < numOrders; ++i) {
    orderIds[i] = i;
    getOrderBook()->addOrder(1, i, i, BUY, 100 + (i % 10), 50);
  }
  std::random_device rd;
  std::mt19937 gen(rd());
  std::ranges::shuffle(orderIds, gen);
  auto start = std::chrono::high_resolution_clock::now();
  for (models::OrderId i = 0; i < numOrders; ++i) {
    getOrderBook()->removeOrder(1, orderIds[i]);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro> duration = end - start;
  EXPECT_TRUE(duration.count() / numOrders < 0.04);
}

TEST_F(OrderBookTest, PerformanceTestMatchOrder) {
  const int numOrders = 500000;
  for (models::OrderId i = 0; i < numOrders; ++i) {
    getOrderBook()->addOrder(1, i, i, BUY, 100 + (i % 10), 50);
  }
  int totalMatches{};
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < numOrders; ++i) {
    if (auto result = getOrderBook()->match(2, 1, SELL, 100 + (i % 10), 1000);
        !result.matches_.empty()) {
      totalMatches += result.matches_.size();
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro> duration = end - start;
  auto throughput =
      static_cast<double>(totalMatches) / (duration.count() / 1'000'000.0);
  EXPECT_TRUE(duration.count() / numOrders < 0.04);
  EXPECT_EQ(totalMatches, numOrders);
  EXPECT_TRUE(throughput > 50'000'000);
}
#endif

} // namespace stockex::engine
