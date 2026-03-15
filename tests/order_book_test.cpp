#include <algorithm>
#include <chrono>
#include <memory>

#include <gtest/gtest.h>
#include <random>

#include "engine/order_book.hpp"
#include "models/basic_types.hpp"

namespace stockex::engine {

using enum models::Side;
class OrderBookTest : public ::testing::Test {
protected:
  void SetUp() override { book_ = std::make_unique<OrderBook>(1); }

  auto AddOrderAndVerify(models::ClientId clientId, models::Side side,
                         models::Price price, models::Quantity qty) const
      -> models::OrderId {
    auto result = book_->addOrder(clientId, side, price, qty);
    EXPECT_TRUE(result.has_value());
    auto orderId = *result;
    auto &orderInfo = book_->getOrder(orderId);
    const auto *priceLevel = book_->getPriceLevel(price);
    auto bOrder = priceLevel->orders_.last();
    EXPECT_EQ(bOrder->clientId_, clientId);
    EXPECT_EQ(bOrder->orderId_, orderId);
    EXPECT_EQ(priceLevel->side_, side);
    EXPECT_EQ(orderInfo.price_, price);
    EXPECT_EQ(bOrder->qty_, qty);
    return orderId;
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
  AddOrderAndVerify(1, BUY, 100, 50);
}

TEST_F(OrderBookTest, AddSingleSellOrder) {
  AddOrderAndVerify(1, SELL, 200, 30);
}

TEST_F(OrderBookTest, AddMultipleOrdersSamePriceLevel) {
  auto id1 = AddOrderAndVerify(1, BUY, 100, 50);
  auto id2 = AddOrderAndVerify(1, BUY, 100, 30);
  const auto &order1 = getOrderBook()->getOrder(id1);
  const auto &order2 = getOrderBook()->getOrder(id2);
  EXPECT_EQ(order1.queueHandle_.index_, 0);
  EXPECT_EQ(order2.queueHandle_.index_, 1);
}

TEST_F(OrderBookTest, AddOrdersDifferentPriceLevels) {
  AddOrderAndVerify(1, BUY, 100, 50);
  AddOrderAndVerify(1, BUY, 101, 30);
  models::PriceLevel *bestBid = getOrderBook()->getPriceLevel(101);
  ASSERT_NE(bestBid, nullptr);
  EXPECT_EQ(bestBid->price_, 101);
  EXPECT_EQ(bestBid->next_->price_, 100);
}

TEST_F(OrderBookTest, RemoveOrder) {
  auto id = AddOrderAndVerify(1, BUY, 100, 50);
  EXPECT_TRUE(getOrderBook()->removeOrder(id).has_value());
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, RemoveOrderFromMultiOrderPriceLevel) {
  auto id1 = AddOrderAndVerify(1, BUY, 100, 50);
  auto id2 = AddOrderAndVerify(1, BUY, 100, 30);
  EXPECT_TRUE(getOrderBook()->removeOrder(id1).has_value());
  EXPECT_EQ(getOrderBook()->getPriceLevel(100)->orders_.front()->qty_, 30);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100)->orders_.front()->orderId_, id2);
}

TEST_F(OrderBookTest, MatchSingleFullFill) {
  auto sellId = AddOrderAndVerify(1, SELL, 100, 50);
  auto result = getOrderBook()->match(2, BUY, 100, 50);
  ASSERT_EQ(result.matches_.size(), 1);
  VerifyMatchResult(result.matches_[0], 1, sellId, 100, 50, 0, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 0);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, MatchSinglePartialFillIncoming) {
  auto sellId = AddOrderAndVerify(1, SELL, 100, 30);
  auto result = getOrderBook()->match(2, BUY, 100, 50);
  ASSERT_EQ(result.matches_.size(), 1);
  VerifyMatchResult(result.matches_[0], 1, sellId, 100, 30, 0, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 20);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, MatchSinglePartialFillResting) {
  auto sellId = AddOrderAndVerify(1, SELL, 100, 50);
  auto result = getOrderBook()->match(2, BUY, 100, 30);
  ASSERT_EQ(result.matches_.size(), 1);
  VerifyMatchResult(result.matches_[0], 1, sellId, 100, 30, 20, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 0);
  auto *priceLevel = getOrderBook()->getPriceLevel(100);
  ASSERT_NE(priceLevel, nullptr);
  EXPECT_EQ(priceLevel->orders_.front()->qty_, 20);
}

TEST_F(OrderBookTest, MatchMultipleOrdersSamePriceLevel) {
  auto sellId1 = AddOrderAndVerify(1, SELL, 100, 20);
  auto sellId2 = AddOrderAndVerify(1, SELL, 100, 20);
  auto result = getOrderBook()->match(2, BUY, 100, 50);
  ASSERT_EQ(result.matches_.size(), 2);
  VerifyMatchResult(result.matches_[0], 2, sellId1, 100, 20, 0, 2, 1, BUY, SELL);
  VerifyMatchResult(result.matches_[1], 2, sellId2, 100, 20, 0, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 10);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, MatchMultiplePriceLevels) {
  auto sellId1 = AddOrderAndVerify(1, SELL, 100, 20);
  auto sellId2 = AddOrderAndVerify(1, SELL, 99, 20);
  auto result = getOrderBook()->match(2, BUY, 100, 50);
  ASSERT_EQ(result.matches_.size(), 2);
  VerifyMatchResult(result.matches_[0], 2, sellId2, 99, 20, 0, 2, 1, BUY, SELL);
  VerifyMatchResult(result.matches_[1], 2, sellId1, 100, 20, 0, 2, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 10);
  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
  EXPECT_EQ(getOrderBook()->getPriceLevel(99), nullptr);
}

TEST_F(OrderBookTest, NoMatchPriceMismatch) {
  AddOrderAndVerify(1, SELL, 101, 50);
  auto result = getOrderBook()->match(2, BUY, 100, 50);
  EXPECT_EQ(result.matches_.size(), 0);
  EXPECT_EQ(result.remainingQuantity_, 50);
  auto *priceLevel = getOrderBook()->getPriceLevel(101);
  ASSERT_NE(priceLevel, nullptr);
  EXPECT_EQ(priceLevel->orders_.front()->qty_, 50);
}

TEST_F(OrderBookTest, MatchMaxEventsLimit) {
  std::vector<models::OrderId> ids;
  for (std::size_t i = 0; i < models::MAX_MATCH_EVENTS + 1; ++i) {
    ids.push_back(AddOrderAndVerify(1, SELL, 100, 10));
  }
  auto result = getOrderBook()->match(2, BUY, 100, 10000);
  EXPECT_EQ(result.matches_.size(), models::MAX_MATCH_EVENTS);
  EXPECT_EQ(result.overflow_, true);
  EXPECT_EQ(result.remainingQuantity_, 10000 - models::MAX_MATCH_EVENTS * 10);
  EXPECT_NE(getOrderBook()->getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, ComplexScenario) {
  auto sellId1 = AddOrderAndVerify(1, SELL, 100, 25);
  AddOrderAndVerify(1, SELL, 101, 30);
  auto sellId3 = AddOrderAndVerify(1, SELL, 99, 40);
  AddOrderAndVerify(2, BUY, 98, 50);
  AddOrderAndVerify(2, BUY, 97, 60);

  auto result = getOrderBook()->match(3, BUY, 100, 100);
  ASSERT_EQ(result.matches_.size(), 2);
  VerifyMatchResult(result.matches_[0], 5, sellId3, 99, 40, 0, 3, 1, BUY, SELL);
  VerifyMatchResult(result.matches_[1], 5, sellId1, 100, 25, 0, 3, 1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 35);

  EXPECT_EQ(getOrderBook()->getPriceLevel(100), nullptr);
  EXPECT_EQ(getOrderBook()->getPriceLevel(99), nullptr);
  auto *priceLevel = getOrderBook()->getPriceLevel(101);
  ASSERT_NE(priceLevel, nullptr);
  EXPECT_EQ(priceLevel->orders_.front()->qty_, 30);
}

TEST_F(OrderBookTest, FreeListReusesIds) {
  auto id1 = *getOrderBook()->addOrder(1, BUY, 100, 50);
  (void)getOrderBook()->addOrder(1, BUY, 101, 30);
  getOrderBook()->removeOrder(id1);
  auto id3 = *getOrderBook()->addOrder(1, BUY, 102, 20);
  EXPECT_EQ(id3, id1);
}

TEST_F(OrderBookTest, AddOrderOrderIdExhausted) {
  // Create a book with capacity for 1 order
  OrderBook tinyBook(1, 1);
  auto r1 = tinyBook.addOrder(1, BUY, 100, 10);
  ASSERT_TRUE(r1.has_value());
  // Pool is now full and free list is empty
  auto r2 = tinyBook.addOrder(1, BUY, 101, 10);
  ASSERT_FALSE(r2.has_value());
  EXPECT_EQ(r2.error(), OrderBookError::OrderIdExhausted);
}

TEST_F(OrderBookTest, RemoveOrderInvalidId) {
  // Use an ID larger than orders_.size() to guarantee out-of-bounds
  OrderBook tinyBook(1, 1);
  auto result = tinyBook.removeOrder(999);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OrderBookError::InvalidOrderId);
}

TEST_F(OrderBookTest, RemoveAlreadyRemovedOrderReturnsError) {
  auto id = *getOrderBook()->addOrder(1, BUY, 100, 50);
  ASSERT_TRUE(getOrderBook()->removeOrder(id).has_value());
  // Second remove of the same id must fail
  auto result = getOrderBook()->removeOrder(id);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OrderBookError::InvalidOrderId);
}

TEST_F(OrderBookTest, RemoveInRangeButUnusedIdReturnsError) {
  // Add one order so that id 0 is allocated; id 1 is in range but never used
  (void)getOrderBook()->addOrder(1, BUY, 100, 50);
  auto result = getOrderBook()->removeOrder(1);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OrderBookError::InvalidOrderId);
}

#ifdef NDEBUG
TEST_F(OrderBookTest, PerformanceTestAddOrder) {
  const int numOrders = 500000;
  for (int i = 0; i < numOrders; ++i) {
    (void)getOrderBook()->addOrder(1, BUY, 100 + (i % 10), 50);
  }
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < numOrders; ++i) {
    (void)getOrderBook()->addOrder(1, BUY, 100 + (i % 10), 50);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro> duration = end - start;
  EXPECT_TRUE(duration.count() / numOrders < 0.04);
}

TEST_F(OrderBookTest, PerformanceTestRemoveOrder) {
  const int numOrders = 500000;
  std::vector<models::OrderId> orderIds;
  orderIds.reserve(numOrders);

  for (int i = 0; i < numOrders; ++i) {
    orderIds.push_back(*getOrderBook()->addOrder(1, BUY, 100 + (i % 10), 50));
  }
  std::random_device rd;
  std::mt19937 gen(rd());
  std::ranges::shuffle(orderIds, gen);
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < numOrders; ++i) {
    getOrderBook()->removeOrder(orderIds[i]);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro> duration = end - start;
  EXPECT_TRUE(duration.count() / numOrders < 0.04);
}

TEST_F(OrderBookTest, PerformanceTestMatchOrder) {
  const int numOrders = 500000;
  for (int i = 0; i < numOrders; ++i) {
    (void)getOrderBook()->addOrder(1, BUY, 100 + (i % 10), 50);
  }
  int totalMatches{};
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < numOrders; ++i) {
    if (auto result = getOrderBook()->match(2, SELL, 100 + (i % 10), 1000);
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
