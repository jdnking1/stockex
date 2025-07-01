#include <deque>
#include <gtest/gtest.h>
#include <models/basic_types.hpp>

#include "models/order_queue.hpp"

namespace stockex::models {
class OrderQueueTest : public ::testing::Test {
public:
  OrderQueue &getQueue() { return queue; }

private:
  OrderQueue queue{10};
};

TEST_F(OrderQueueTest, IsInitiallyEmpty) {
  EXPECT_EQ(getQueue().size(), 0);
  EXPECT_TRUE(getQueue().empty());
  EXPECT_EQ(getQueue().front(), nullptr);
  EXPECT_EQ(getQueue().last(), nullptr);
}

TEST_F(OrderQueueTest, PushAndFront) {
  ASSERT_TRUE(getQueue().empty());

  BasicOrder order1 = {101, 10, 1};
  getQueue().push(order1);

  EXPECT_FALSE(getQueue().empty());
  EXPECT_EQ(getQueue().size(), 1);
  ASSERT_NE(getQueue().front(), nullptr);
  EXPECT_EQ(*getQueue().front(), order1);
}

TEST_F(OrderQueueTest, PushAndLast) {
  BasicOrder order1 = {101, 10, 1};
  BasicOrder order2 = {102, 20, 2};
  getQueue().push(order1);
  getQueue().push(order2);

  EXPECT_EQ(getQueue().size(), 2);
  ASSERT_NE(getQueue().last(), nullptr);
  EXPECT_EQ(*getQueue().last(), order2);
}

TEST_F(OrderQueueTest, PopSimple) {
  BasicOrder order1 = {101, 10, 1};
  BasicOrder order2 = {102, 20, 2};
  getQueue().push(order1);
  getQueue().push(order2);

  ASSERT_EQ(*getQueue().front(), order1);

  getQueue().pop();

  EXPECT_EQ(getQueue().size(), 1);
  ASSERT_NE(getQueue().front(), nullptr);
  EXPECT_EQ(*getQueue().front(), order2);

  getQueue().pop();
  EXPECT_TRUE(getQueue().empty());
  EXPECT_EQ(getQueue().front(), nullptr);
}

TEST_F(OrderQueueTest, RemoveFromMiddle) {
  BasicOrder order1 = {101, 10, 1};
  BasicOrder order2 = {102, 20, 2};
  BasicOrder order3 = {103, 30, 3};

  getQueue().push(order1);
  auto pos2 = getQueue().push(order2);
  getQueue().push(order3);

  ASSERT_EQ(getQueue().size(), 3);

  getQueue().remove(pos2);

  EXPECT_EQ(getQueue().size(), 2);
  ASSERT_EQ(*getQueue().front(), order1);

  getQueue().pop();
  ASSERT_EQ(*getQueue().front(), order3);
}

TEST_F(OrderQueueTest, RemoveLast) {
  BasicOrder order1 = {101, 10, 1};
  BasicOrder order2 = {102, 20, 2};
  getQueue().push(order1);
  auto pos2 = getQueue().push(order2);

  ASSERT_EQ(*getQueue().last(), order2);
  getQueue().remove(pos2);

  EXPECT_EQ(getQueue().size(), 1);
  ASSERT_NE(getQueue().last(), nullptr);
  EXPECT_EQ(*getQueue().last(), order1);
}

TEST_F(OrderQueueTest, RemoveFront) {
  BasicOrder order1 = {101, 10, 1};
  BasicOrder order2 = {102, 20, 2};
  auto pos1 = getQueue().push(order1);
  getQueue().push(order2);

  getQueue().remove(pos1);
  EXPECT_EQ(getQueue().size(), 1);
  ASSERT_NE(getQueue().front(), nullptr);
  EXPECT_EQ(*getQueue().front(), order2);
}

TEST_F(OrderQueueTest, CompactionTriggerAndBehavior) {
  OrderQueue q(10, 3);

  q.push({1, 1, 1});
  q.push({2, 2, 2});
  q.push({3, 3, 3});
  q.pop();
  q.pop();
  q.pop();

  ASSERT_EQ(q.size(), 0);

  q.push({4, 4, 4});
  EXPECT_EQ(q.size(), 1);

  BasicOrder *frontOrder = q.front();
  ASSERT_NE(frontOrder, nullptr);
  EXPECT_EQ(frontOrder->orderId_, 4);

  q.pop();
  EXPECT_TRUE(q.empty());
}

TEST_F(OrderQueueTest, RemoveAfterCompaction) {
  OrderQueue q(10, 2);

  q.push({1, 1, 1});
  q.push({2, 2, 2});
  q.pop();
  q.pop();

  ASSERT_EQ(q.front(), nullptr);

  auto posC = q.push({3, 3, 3});
  q.push({4, 4, 4});

  q.pop();
  q.pop();

  ASSERT_EQ(q.front(), nullptr);

  q.push({5, 5, 5});

  q.remove(posC);

  EXPECT_EQ(q.size(), 1);
  ASSERT_NE(q.front(), nullptr);
  EXPECT_EQ(q.front()->orderId_, 5);
}

TEST_F(OrderQueueTest, StressTestWithMixedOperations) {
  OrderQueue q(100, 50);
  std::deque<BasicOrder> model;

  std::vector<QueuePosition> positions;

  for (int i = 0; i < 200; ++i) {
    BasicOrder order = {static_cast<OrderId>(i), static_cast<Quantity>(i * 10),
                        static_cast<ClientId>(i)};
    positions.push_back(q.push(order));
    model.push_back(order);
  }

  for (int i = 50; i < 100; ++i) {
    q.remove(positions[i]);
    std::erase_if(model, [i](const BasicOrder &o) {
      return o.orderId_ == static_cast<OrderId>(i);
    });
  }

  for (int i = 0; i < 50; ++i) {
    q.pop();
    if (!model.empty()) {
      model.pop_front();
    }
  }

  ASSERT_EQ(q.size(), model.size());

  while (!q.empty() && !model.empty()) {
    ASSERT_NE(q.front(), nullptr);
    EXPECT_EQ(*q.front(), model.front());
    q.pop();
    model.pop_front();
  }

  EXPECT_TRUE(q.empty());
  EXPECT_TRUE(model.empty());
}
} // namespace stockex::models
