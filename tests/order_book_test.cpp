// Comprehensive unit tests for the OrderBook matching engine.
//
// Test groups:
//  1.  Empty book behaviour
//  2.  Adding orders
//  3.  Removing (cancelling) orders
//  4.  Price-level ordering (BID descending, ASK ascending)
//  5.  Matching – BUY incoming vs SELL resting
//  6.  Matching – SELL incoming vs BUY resting
//  7.  Price-time (FIFO) priority
//  8.  MatchResultSet fields
//  9.  Multi-client scenarios
//  10. State consistency across sequences of add / cancel / match

#include <gtest/gtest.h>

#include "engine/order_book.hpp"
#include "models/basic_types.hpp"
#include "models/constants.hpp"

namespace stockex::engine {

using enum models::Side;

// ============================================================
// Fixture
// ============================================================

class OrderBookTest : public ::testing::Test {
protected:
  // Constants used throughout the suite
  static constexpr models::InstrumentId kInstrumentId = 1;
  static constexpr models::ClientId     kClient1 = 1;
  static constexpr models::ClientId     kClient2 = 2;
  static constexpr models::ClientId     kClient3 = 3;
  static constexpr models::ClientId     kClient4 = 4;

  void SetUp() override { book_ = std::make_unique<OrderBook>(kInstrumentId); }

  OrderBook& book() { return *book_; }

  void addOrder(models::ClientId     clientId,
                models::OrderId      clientOrderId,
                models::OrderId      marketOrderId,
                models::Side         side,
                models::Price        price,
                models::Quantity     qty) {
    book_->addOrder(clientId, clientOrderId, marketOrderId, side, price, qty);
  }

  // Verify that a price level exists and its front order has the expected qty.
  void expectLevel(models::Price price, models::Side side,
                   models::Quantity frontQty) const {
    const auto* level = book_->getPriceLevel(price);
    ASSERT_NE(level, nullptr) << "Expected price level at price " << price;
    EXPECT_EQ(level->side_,  side);
    EXPECT_EQ(level->price_, price);
    ASSERT_NE(level->orders_.front(), nullptr);
    EXPECT_EQ(level->orders_.front()->qty_, frontQty);
  }

  // Verify that no price level exists at the given price.
  void expectNoLevel(models::Price price) const {
    EXPECT_EQ(book_->getPriceLevel(price), nullptr)
        << "Expected no price level at price " << price;
  }

  // Verify every field of a single MatchResult entry.
  static void checkMatch(const MatchResult&   r,
                         models::OrderId      incomingId,
                         models::OrderId      matchedId,
                         models::Price        price,
                         models::Quantity     matchedQty,
                         models::Quantity     matchedRemaining,
                         models::ClientId     incomingClient,
                         models::ClientId     matchedClient,
                         models::Side         incomingSide,
                         models::Side         matchedSide) {
    EXPECT_EQ(r.incomingOrderId_,         incomingId);
    EXPECT_EQ(r.matchedOrderId_,          matchedId);
    EXPECT_EQ(r.price_,                   price);
    EXPECT_EQ(r.quantity_,                matchedQty);
    EXPECT_EQ(r.matchedOrderRemainingQty_, matchedRemaining);
    EXPECT_EQ(r.incomingClientId_,        incomingClient);
    EXPECT_EQ(r.matchedClientId_,         matchedClient);
    EXPECT_EQ(r.incomingOrderSide_,       incomingSide);
    EXPECT_EQ(r.matchedOrderSide_,        matchedSide);
  }

private:
  std::unique_ptr<OrderBook> book_;
};

// ============================================================
// 1. Empty book behaviour
// ============================================================

TEST_F(OrderBookTest, EmptyBook_BuyMatch_ReturnsNoMatchesFullRemainder) {
  auto result = book().match(kClient1, 1, BUY, 100, 50);
  EXPECT_EQ(result.matches_.size(), 0u);
  EXPECT_EQ(result.remainingQuantity_, 50);
  EXPECT_FALSE(result.overflow_);
}

TEST_F(OrderBookTest, EmptyBook_SellMatch_ReturnsNoMatchesFullRemainder) {
  auto result = book().match(kClient1, 1, SELL, 100, 50);
  EXPECT_EQ(result.matches_.size(), 0u);
  EXPECT_EQ(result.remainingQuantity_, 50);
  EXPECT_FALSE(result.overflow_);
}

// ============================================================
// 2. Adding orders
// ============================================================

TEST_F(OrderBookTest, AddBuyOrder_PriceLevelCreatedWithCorrectSideAndQty) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  expectLevel(100, BUY, 50);
}

TEST_F(OrderBookTest, AddSellOrder_PriceLevelCreatedWithCorrectSideAndQty) {
  addOrder(kClient1, 10, 10, SELL, 200, 30);
  expectLevel(200, SELL, 30);
}

TEST_F(OrderBookTest, AddOrder_OrderInfoStoresMarketOrderIdAndPrice) {
  addOrder(kClient1, 42, 99, BUY, 150, 75);
  const auto& info = book().getOrder(kClient1, 42);
  EXPECT_EQ(info.marketOrderId_, 99);
  EXPECT_EQ(info.price_,         150);
}

TEST_F(OrderBookTest, AddMultipleBuyOrders_SamePriceLevel_AllEnqueued) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  addOrder(kClient1, 11, 11, BUY, 100, 30);

  const auto* level = book().getPriceLevel(100);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.size(), 2u);
}

TEST_F(OrderBookTest, AddMultipleSellOrders_SamePriceLevel_AllEnqueued) {
  addOrder(kClient1, 10, 10, SELL, 200, 20);
  addOrder(kClient1, 11, 11, SELL, 200, 40);

  const auto* level = book().getPriceLevel(200);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.size(), 2u);
}

TEST_F(OrderBookTest, AddBuyAndSellOrders_SeparatePriceLevels_NoCrossing) {
  addOrder(kClient1, 10, 10, BUY,  100, 50);
  addOrder(kClient2, 20, 20, SELL, 101, 30);

  expectLevel(100, BUY,  50);
  expectLevel(101, SELL, 30);
}

// ============================================================
// 3. Removing (cancelling) orders
// ============================================================

TEST_F(OrderBookTest, CancelOrder_OnlyOrder_PriceLevelRemoved) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  book().removeOrder(kClient1, 10);
  expectNoLevel(100);
}

TEST_F(OrderBookTest, CancelOrder_FirstOfTwo_FrontUpdatesToSecond) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  addOrder(kClient1, 11, 11, BUY, 100, 30);
  book().removeOrder(kClient1, 10);

  const auto* level = book().getPriceLevel(100);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.size(), 1u);
  EXPECT_EQ(level->orders_.front()->orderId_, 11);
  EXPECT_EQ(level->orders_.front()->qty_,     30);
}

TEST_F(OrderBookTest, CancelOrder_LastOfTwo_FrontPreserved) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  addOrder(kClient1, 11, 11, BUY, 100, 30);
  book().removeOrder(kClient1, 11);

  const auto* level = book().getPriceLevel(100);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.size(), 1u);
  EXPECT_EQ(level->orders_.front()->orderId_, 10);
  EXPECT_EQ(level->orders_.front()->qty_,     50);
}

TEST_F(OrderBookTest, CancelOrder_MiddleOfThree_EndOrdersIntact) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  addOrder(kClient1, 11, 11, BUY, 100, 30);
  addOrder(kClient1, 12, 12, BUY, 100, 20);
  book().removeOrder(kClient1, 11);

  const auto* level = book().getPriceLevel(100);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.size(), 2u);
  // Head should still be the originally-first order
  EXPECT_EQ(level->orders_.front()->orderId_, 10);
}

TEST_F(OrderBookTest, CancelBestBid_NextBidBecomesMatchable) {
  addOrder(kClient1, 10, 10, BUY, 105, 50); // best bid
  addOrder(kClient1, 11, 11, BUY, 100, 40); // second bid

  book().removeOrder(kClient1, 10);
  expectNoLevel(105);

  // A SELL at 100 should now match against the 100-level bid
  auto result = book().match(kClient2, 99, SELL, 100, 40);
  ASSERT_EQ(result.matches_.size(), 1u);
  EXPECT_EQ(result.matches_[0].price_, 100);
  EXPECT_EQ(result.remainingQuantity_, 0);
}

TEST_F(OrderBookTest, CancelBestAsk_NextAskBecomesMatchable) {
  addOrder(kClient1, 10, 10, SELL, 95,  20); // best ask
  addOrder(kClient1, 11, 11, SELL, 100, 20); // second ask

  book().removeOrder(kClient1, 10);
  expectNoLevel(95);

  // A BUY at 100 should now match against the 100-level ask
  auto result = book().match(kClient2, 99, BUY, 100, 20);
  ASSERT_EQ(result.matches_.size(), 1u);
  EXPECT_EQ(result.matches_[0].price_, 100);
  EXPECT_EQ(result.remainingQuantity_, 0);
}

// ============================================================
// 4. Price-level ordering (BID descending, ASK ascending)
// ============================================================

TEST_F(OrderBookTest, BidOrdering_HighestPriceMatchesFirst) {
  // Add in non-sorted order to confirm the sorted structure holds
  addOrder(kClient1, 10, 10, BUY, 100, 5);
  addOrder(kClient1, 11, 11, BUY, 105, 5);
  addOrder(kClient1, 12, 12, BUY, 98,  5);

  // SELL sweeps all three; prices should appear highest-first
  auto result = book().match(kClient2, 99, SELL, 90, 20);
  ASSERT_EQ(result.matches_.size(), 3u);
  EXPECT_EQ(result.matches_[0].price_, 105);
  EXPECT_EQ(result.matches_[1].price_, 100);
  EXPECT_EQ(result.matches_[2].price_, 98);
}

TEST_F(OrderBookTest, AskOrdering_LowestPriceMatchesFirst) {
  addOrder(kClient1, 10, 10, SELL, 100, 5);
  addOrder(kClient1, 11, 11, SELL, 95,  5);
  addOrder(kClient1, 12, 12, SELL, 102, 5);

  // BUY sweeps all three; prices should appear lowest-first
  auto result = book().match(kClient2, 99, BUY, 110, 20);
  ASSERT_EQ(result.matches_.size(), 3u);
  EXPECT_EQ(result.matches_[0].price_, 95);
  EXPECT_EQ(result.matches_[1].price_, 100);
  EXPECT_EQ(result.matches_[2].price_, 102);
}

// ============================================================
// 5. Matching – BUY incoming vs SELL resting
// ============================================================

TEST_F(OrderBookTest, Match_Buy_AskAboveBidPrice_NoMatch) {
  addOrder(kClient1, 10, 10, SELL, 105, 50);
  auto result = book().match(kClient2, 99, BUY, 100, 50);

  EXPECT_EQ(result.matches_.size(), 0u);
  EXPECT_EQ(result.remainingQuantity_, 50);
  expectLevel(105, SELL, 50); // resting order untouched
}

TEST_F(OrderBookTest, Match_Buy_ExactFill_LevelRemoved) {
  addOrder(kClient1, 10, 10, SELL, 100, 50);
  auto result = book().match(kClient2, 99, BUY, 100, 50);

  ASSERT_EQ(result.matches_.size(), 1u);
  checkMatch(result.matches_[0],
             /*incomingId=*/99,  /*matchedId=*/10,
             /*price=*/100,      /*matchedQty=*/50, /*matchedRemaining=*/0,
             kClient2, kClient1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 0);
  EXPECT_FALSE(result.overflow_);
  expectNoLevel(100);
}

TEST_F(OrderBookTest, Match_Buy_RestingPartialFill_IncomingFullyConsumed) {
  // Resting SELL has more qty than the incoming BUY
  addOrder(kClient1, 10, 10, SELL, 100, 50);
  auto result = book().match(kClient2, 99, BUY, 100, 30);

  ASSERT_EQ(result.matches_.size(), 1u);
  checkMatch(result.matches_[0], 99, 10, 100, 30, /*remaining=*/20,
             kClient2, kClient1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 0);

  // 20 units remain in the book
  const auto* level = book().getPriceLevel(100);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.front()->qty_, 20);
}

TEST_F(OrderBookTest, Match_Buy_IncomingPartialFill_RestingFullyConsumed) {
  // Resting SELL has fewer qty than the incoming BUY
  addOrder(kClient1, 10, 10, SELL, 100, 30);
  auto result = book().match(kClient2, 99, BUY, 100, 50);

  ASSERT_EQ(result.matches_.size(), 1u);
  checkMatch(result.matches_[0], 99, 10, 100, 30, 0, kClient2, kClient1, BUY, SELL);
  EXPECT_EQ(result.remainingQuantity_, 20);
  expectNoLevel(100);
}

TEST_F(OrderBookTest, Match_Buy_MultipleLevels_BestAskConsumedFirst) {
  // Two SELL levels; lower (better) ask price should be consumed first
  addOrder(kClient1, 10, 10, SELL, 101, 20);
  addOrder(kClient1, 11, 11, SELL, 99,  20);
  auto result = book().match(kClient2, 99, BUY, 101, 50);

  ASSERT_EQ(result.matches_.size(), 2u);
  EXPECT_EQ(result.matches_[0].price_,          99);
  EXPECT_EQ(result.matches_[0].matchedOrderId_, 11);
  EXPECT_EQ(result.matches_[1].price_,          101);
  EXPECT_EQ(result.matches_[1].matchedOrderId_, 10);
  EXPECT_EQ(result.remainingQuantity_, 10);
}

TEST_F(OrderBookTest, Match_Buy_PriceLimit_OnlyMatchableLevelsConsumed) {
  // Three SELL levels: 99, 101, 103. BUY at 101 must NOT touch the 103 level.
  addOrder(kClient1, 10, 10, SELL, 99,  10);
  addOrder(kClient1, 11, 11, SELL, 101, 10);
  addOrder(kClient1, 12, 12, SELL, 103, 10);
  auto result = book().match(kClient2, 99, BUY, 101, 100);

  ASSERT_EQ(result.matches_.size(), 2u);
  EXPECT_EQ(result.matches_[0].price_, 99);
  EXPECT_EQ(result.matches_[1].price_, 101);
  EXPECT_EQ(result.remainingQuantity_, 80);
  expectLevel(103, SELL, 10); // untouched
}

TEST_F(OrderBookTest, Match_Buy_SweepEntireAskSide) {
  // BUY with high price and large qty clears all SELL levels
  for (models::OrderId i = 0; i < 5; ++i) {
    addOrder(kClient1, i, i, SELL, static_cast<models::Price>(100 + i), 10);
  }
  auto result = book().match(kClient2, 999, BUY, 110, 50);

  ASSERT_EQ(result.matches_.size(), 5u);
  EXPECT_EQ(result.remainingQuantity_, 0);
  for (models::Price p = 100; p <= 104; ++p) {
    expectNoLevel(p);
  }
}

// ============================================================
// 6. Matching – SELL incoming vs BUY resting
// ============================================================

TEST_F(OrderBookTest, Match_Sell_BidBelowAskPrice_NoMatch) {
  addOrder(kClient1, 10, 10, BUY, 95, 50);
  auto result = book().match(kClient2, 99, SELL, 100, 50);

  EXPECT_EQ(result.matches_.size(), 0u);
  EXPECT_EQ(result.remainingQuantity_, 50);
  expectLevel(95, BUY, 50);
}

TEST_F(OrderBookTest, Match_Sell_ExactFill_LevelRemoved) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  auto result = book().match(kClient2, 99, SELL, 100, 50);

  ASSERT_EQ(result.matches_.size(), 1u);
  checkMatch(result.matches_[0], 99, 10, 100, 50, 0, kClient2, kClient1, SELL, BUY);
  EXPECT_EQ(result.remainingQuantity_, 0);
  EXPECT_FALSE(result.overflow_);
  expectNoLevel(100);
}

TEST_F(OrderBookTest, Match_Sell_RestingPartialFill_IncomingFullyConsumed) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  auto result = book().match(kClient2, 99, SELL, 100, 30);

  ASSERT_EQ(result.matches_.size(), 1u);
  checkMatch(result.matches_[0], 99, 10, 100, 30, 20, kClient2, kClient1, SELL, BUY);
  EXPECT_EQ(result.remainingQuantity_, 0);

  const auto* level = book().getPriceLevel(100);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.front()->qty_, 20);
}

TEST_F(OrderBookTest, Match_Sell_IncomingPartialFill_RestingFullyConsumed) {
  addOrder(kClient1, 10, 10, BUY, 100, 30);
  auto result = book().match(kClient2, 99, SELL, 100, 50);

  ASSERT_EQ(result.matches_.size(), 1u);
  checkMatch(result.matches_[0], 99, 10, 100, 30, 0, kClient2, kClient1, SELL, BUY);
  EXPECT_EQ(result.remainingQuantity_, 20);
  expectNoLevel(100);
}

TEST_F(OrderBookTest, Match_Sell_MultipleLevels_BestBidConsumedFirst) {
  // Two BUY levels; higher (better) bid price consumed first
  addOrder(kClient1, 10, 10, BUY, 99,  20);
  addOrder(kClient1, 11, 11, BUY, 101, 20);
  auto result = book().match(kClient2, 99, SELL, 99, 50);

  ASSERT_EQ(result.matches_.size(), 2u);
  EXPECT_EQ(result.matches_[0].price_,          101);
  EXPECT_EQ(result.matches_[0].matchedOrderId_, 11);
  EXPECT_EQ(result.matches_[1].price_,          99);
  EXPECT_EQ(result.matches_[1].matchedOrderId_, 10);
  EXPECT_EQ(result.remainingQuantity_, 10);
}

TEST_F(OrderBookTest, Match_Sell_PriceLimit_OnlyMatchableLevelsConsumed) {
  // Three BUY levels: 97, 100, 103. SELL at 100 must NOT touch the 97 level.
  addOrder(kClient1, 10, 10, BUY, 103, 10);
  addOrder(kClient1, 11, 11, BUY, 100, 10);
  addOrder(kClient1, 12, 12, BUY, 97,  10);
  auto result = book().match(kClient2, 99, SELL, 100, 100);

  ASSERT_EQ(result.matches_.size(), 2u);
  EXPECT_EQ(result.matches_[0].price_, 103);
  EXPECT_EQ(result.matches_[1].price_, 100);
  EXPECT_EQ(result.remainingQuantity_, 80);
  expectLevel(97, BUY, 10); // untouched
}

TEST_F(OrderBookTest, Match_Sell_SweepEntireBidSide) {
  for (models::OrderId i = 0; i < 5; ++i) {
    addOrder(kClient1, i, i, BUY, static_cast<models::Price>(100 + i), 10);
  }
  auto result = book().match(kClient2, 999, SELL, 100, 50);

  ASSERT_EQ(result.matches_.size(), 5u);
  EXPECT_EQ(result.remainingQuantity_, 0);
  for (models::Price p = 100; p <= 104; ++p) {
    expectNoLevel(p);
  }
}

// ============================================================
// 7. Price-time (FIFO) priority
// ============================================================

TEST_F(OrderBookTest, FIFO_Buy_FirstInFirstMatched) {
  // Two SELL orders at the same price: first-added must match first.
  addOrder(kClient1, 10, 10, SELL, 100, 20);
  addOrder(kClient1, 11, 11, SELL, 100, 20);
  auto result = book().match(kClient2, 99, BUY, 100, 25);

  ASSERT_EQ(result.matches_.size(), 2u);
  // Order 10 (first-in) is fully consumed for 20 units
  EXPECT_EQ(result.matches_[0].matchedOrderId_, 10);
  EXPECT_EQ(result.matches_[0].quantity_,        20);
  EXPECT_EQ(result.matches_[0].matchedOrderRemainingQty_, 0);
  // Order 11 (second-in) partially consumed for 5 units, 15 remain
  EXPECT_EQ(result.matches_[1].matchedOrderId_, 11);
  EXPECT_EQ(result.matches_[1].quantity_,        5);
  EXPECT_EQ(result.matches_[1].matchedOrderRemainingQty_, 15);
}

TEST_F(OrderBookTest, FIFO_Sell_FirstInFirstMatched) {
  addOrder(kClient1, 10, 10, BUY, 100, 20);
  addOrder(kClient1, 11, 11, BUY, 100, 20);
  auto result = book().match(kClient2, 99, SELL, 100, 25);

  ASSERT_EQ(result.matches_.size(), 2u);
  EXPECT_EQ(result.matches_[0].matchedOrderId_, 10);
  EXPECT_EQ(result.matches_[0].quantity_,        20);
  EXPECT_EQ(result.matches_[1].matchedOrderId_, 11);
  EXPECT_EQ(result.matches_[1].quantity_,        5);
  EXPECT_EQ(result.matches_[1].matchedOrderRemainingQty_, 15);
}

TEST_F(OrderBookTest, FIFO_AfterCancelHead_SecondOrderBecomesFirst) {
  addOrder(kClient1, 10, 10, SELL, 100, 20);
  addOrder(kClient1, 11, 11, SELL, 100, 30);

  // Cancel the head order; order 11 should now be matched first
  book().removeOrder(kClient1, 10);

  auto result = book().match(kClient2, 99, BUY, 100, 10);
  ASSERT_EQ(result.matches_.size(), 1u);
  EXPECT_EQ(result.matches_[0].matchedOrderId_, 11);
  EXPECT_EQ(result.matches_[0].matchedOrderRemainingQty_, 20);
}

// ============================================================
// 8. MatchResultSet fields
// ============================================================

TEST_F(OrderBookTest, MatchResult_AllFieldsPopulatedCorrectly) {
  addOrder(kClient1, 10, 10, SELL, 100, 50);
  auto result = book().match(kClient2, 99, BUY, 100, 50);

  ASSERT_EQ(result.matches_.size(), 1u);
  const auto& m = result.matches_[0];
  EXPECT_EQ(m.incomingOrderId_,          99);
  EXPECT_EQ(m.matchedOrderId_,           10);
  EXPECT_EQ(m.price_,                    100);
  EXPECT_EQ(m.quantity_,                 50);
  EXPECT_EQ(m.matchedOrderRemainingQty_, 0);
  EXPECT_EQ(m.incomingClientId_,         kClient2);
  EXPECT_EQ(m.matchedClientId_,          kClient1);
  EXPECT_EQ(m.incomingOrderSide_,        BUY);
  EXPECT_EQ(m.matchedOrderSide_,         SELL);
}

TEST_F(OrderBookTest, MatchResultSet_InstrumentIdPropagated) {
  addOrder(kClient1, 10, 10, SELL, 100, 50);
  auto result = book().match(kClient2, 99, BUY, 100, 50);
  EXPECT_EQ(result.instrument_, kInstrumentId);
}

TEST_F(OrderBookTest, MatchResultSet_OverflowFalse_WhenUnderLimit) {
  // A handful of matches – well under MAX_MATCH_EVENTS
  for (models::OrderId i = 0; i < 5; ++i) {
    addOrder(kClient1, i, i, SELL, 100, 10);
  }
  auto result = book().match(kClient2, 999, BUY, 100, 50);
  EXPECT_EQ(result.matches_.size(), 5u);
  EXPECT_FALSE(result.overflow_);
  EXPECT_EQ(result.remainingQuantity_, 0);
}

TEST_F(OrderBookTest, MatchResultSet_OverflowTrue_WhenLimitExceeded) {
  // One more resting order than MAX_MATCH_EVENTS so the cap is triggered
  for (models::OrderId i = 0; i < models::MAX_MATCH_EVENTS + 1; ++i) {
    addOrder(kClient1, i, i, SELL, 100, 1);
  }
  auto result = book().match(
      kClient2, 9999, BUY, 100,
      static_cast<models::Quantity>(models::MAX_MATCH_EVENTS + 1));

  EXPECT_EQ(result.matches_.size(), models::MAX_MATCH_EVENTS);
  EXPECT_TRUE(result.overflow_);
  // At least one order must remain in the book
  EXPECT_NE(book().getPriceLevel(100), nullptr);
}

TEST_F(OrderBookTest, MatchResultSet_OverflowFalse_WhenExactlyAtLimit) {
  // Exactly MAX_MATCH_EVENTS resting orders, all consumed – no overflow.
  for (models::OrderId i = 0; i < models::MAX_MATCH_EVENTS; ++i) {
    addOrder(kClient1, i, i, SELL, 100, 1);
  }
  auto result = book().match(
      kClient2, 9999, BUY, 100,
      static_cast<models::Quantity>(models::MAX_MATCH_EVENTS));

  EXPECT_EQ(result.matches_.size(), models::MAX_MATCH_EVENTS);
  EXPECT_FALSE(result.overflow_);
  EXPECT_EQ(result.remainingQuantity_, 0);
  expectNoLevel(100);
}

// ============================================================
// 9. Multi-client scenarios
// ============================================================

TEST_F(OrderBookTest, MultiClient_MatchResultCarriesCorrectClientIds) {
  addOrder(kClient1, 10, 10, SELL, 100, 50);
  auto result = book().match(kClient2, 99, BUY, 100, 50);

  ASSERT_EQ(result.matches_.size(), 1u);
  EXPECT_EQ(result.matches_[0].incomingClientId_, kClient2);
  EXPECT_EQ(result.matches_[0].matchedClientId_,  kClient1);
}

TEST_F(OrderBookTest, MultiClient_ThreeRestingClients_MatchedInFIFOOrder) {
  // Three different clients rest at the same price; one aggressor sweeps them all
  addOrder(kClient1, 10, 10, SELL, 100, 10);
  addOrder(kClient2, 20, 20, SELL, 100, 10);
  addOrder(kClient3, 30, 30, SELL, 100, 10);

  auto result = book().match(kClient4, 99, BUY, 100, 30);
  ASSERT_EQ(result.matches_.size(), 3u);
  EXPECT_EQ(result.matches_[0].matchedClientId_, kClient1);
  EXPECT_EQ(result.matches_[1].matchedClientId_, kClient2);
  EXPECT_EQ(result.matches_[2].matchedClientId_, kClient3);
  EXPECT_EQ(result.remainingQuantity_, 0);
}

TEST_F(OrderBookTest, MultiClient_EachClientOwnsItsOwnOrders) {
  addOrder(kClient1, 10, 10, BUY, 100, 50);
  addOrder(kClient2, 20, 20, BUY, 105, 30);

  // Cancelling kClient1's order must not disturb kClient2's order
  book().removeOrder(kClient1, 10);
  expectNoLevel(100);
  expectLevel(105, BUY, 30);
}

TEST_F(OrderBookTest, MultiClient_SamePriceDifferentClients_IndependentMarketIds) {
  addOrder(kClient1, 10, 42, SELL, 100, 20);
  addOrder(kClient2, 10, 77, SELL, 100, 30); // same clientOrderId, different client

  EXPECT_EQ(book().getOrder(kClient1, 10).marketOrderId_, 42);
  EXPECT_EQ(book().getOrder(kClient2, 10).marketOrderId_, 77);
}

// ============================================================
// 10. State consistency across add / cancel / match sequences
// ============================================================

TEST_F(OrderBookTest, Sequence_CancelAllResting_ThenNoMatch) {
  addOrder(kClient1, 10, 10, SELL, 100, 50);
  book().removeOrder(kClient1, 10);

  auto result = book().match(kClient2, 99, BUY, 100, 50);
  EXPECT_EQ(result.matches_.size(), 0u);
  EXPECT_EQ(result.remainingQuantity_, 50);
}

TEST_F(OrderBookTest, Sequence_FullDrain_ThenBookAcceptsNewOrder) {
  addOrder(kClient1, 10, 10, SELL, 100, 50);
  [[maybe_unused]] auto drain = book().match(kClient2, 99, BUY, 100, 50); // drains the book
  expectNoLevel(100);

  addOrder(kClient1, 11, 11, SELL, 100, 20);
  auto result = book().match(kClient2, 100, BUY, 100, 20);
  ASSERT_EQ(result.matches_.size(), 1u);
  EXPECT_EQ(result.matches_[0].quantity_, 20);
  EXPECT_EQ(result.remainingQuantity_, 0);
}

TEST_F(OrderBookTest, Sequence_PartialMatch_CancelRemainder_LevelGone) {
  addOrder(kClient1, 10, 10, SELL, 100, 50);
  [[maybe_unused]] auto partial = book().match(kClient2, 99, BUY, 100, 30); // 20 units remain

  const auto* level = book().getPriceLevel(100);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.front()->qty_, 20);

  book().removeOrder(kClient1, 10);
  expectNoLevel(100);
}

TEST_F(OrderBookTest, Sequence_BestAskAdvancesAfterPartialLevelDrain) {
  addOrder(kClient1, 10, 10, SELL, 99,  10);
  addOrder(kClient1, 11, 11, SELL, 101, 10);

  // Consume the best ask entirely
  [[maybe_unused]] auto r1 = book().match(kClient2, 98, BUY, 99, 10);
  expectNoLevel(99);

  // The next match should use the new best ask (101)
  auto result = book().match(kClient2, 100, BUY, 101, 10);
  ASSERT_EQ(result.matches_.size(), 1u);
  EXPECT_EQ(result.matches_[0].price_, 101);
}

TEST_F(OrderBookTest, Sequence_BestBidAdvancesAfterPartialLevelDrain) {
  addOrder(kClient1, 10, 10, BUY, 101, 10);
  addOrder(kClient1, 11, 11, BUY, 99,  10);

  [[maybe_unused]] auto r1 = book().match(kClient2, 98, SELL, 101, 10);
  expectNoLevel(101);

  auto result = book().match(kClient2, 100, SELL, 99, 10);
  ASSERT_EQ(result.matches_.size(), 1u);
  EXPECT_EQ(result.matches_[0].price_, 99);
}

TEST_F(OrderBookTest, Sequence_InterleavedAddAndCancel_BookConsistent) {
  addOrder(kClient1, 1, 1, BUY, 100, 10);
  addOrder(kClient1, 2, 2, BUY, 100, 10);
  addOrder(kClient1, 3, 3, BUY, 100, 10);

  book().removeOrder(kClient1, 2); // remove middle

  const auto* level = book().getPriceLevel(100);
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->orders_.size(), 2u);

  // Remaining orders should match in original insertion order (1 before 3)
  auto result = book().match(kClient2, 99, SELL, 100, 20);
  ASSERT_EQ(result.matches_.size(), 2u);
  EXPECT_EQ(result.matches_[0].matchedOrderId_, 1);
  EXPECT_EQ(result.matches_[1].matchedOrderId_, 3);
  EXPECT_EQ(result.remainingQuantity_, 0);
}

} // namespace stockex::engine
