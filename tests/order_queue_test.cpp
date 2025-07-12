#include <gtest/gtest.h>
#include <deque>
#include <vector>


#include "models/order_queue.hpp" 

namespace stockex::models {

constexpr size_t TestChunkSize = 4;

class OrderQueueTest : public ::testing::Test {
public:
  using TestQueue = OrderQueue<TestChunkSize>;
  using Handle = typename TestQueue::Handle;

  TestQueue::Allocator pool_{100};
  std::unique_ptr<TestQueue> queue_;

  void SetUp() override {
    queue_ = std::make_unique<TestQueue>(pool_);
  }

  TestQueue& getQueue() { return *queue_; }
};

TEST_F(OrderQueueTest, IsInitiallyEmpty) {
  EXPECT_EQ(getQueue().size(), 0);
  EXPECT_TRUE(getQueue().empty());
  EXPECT_EQ(getQueue().front(), nullptr);
  EXPECT_EQ(getQueue().last(), nullptr);
}

TEST_F(OrderQueueTest, PushAndFrontAndLast) {
  ASSERT_TRUE(getQueue().empty());

  BasicOrder order1 = {101, 10, 1};
  getQueue().push(order1);

  EXPECT_FALSE(getQueue().empty());
  EXPECT_EQ(getQueue().size(), 1);
  ASSERT_NE(getQueue().front(), nullptr);
  EXPECT_EQ(getQueue().front()->orderId_, order1.orderId_);
  ASSERT_NE(getQueue().last(), nullptr);
  EXPECT_EQ(getQueue().last()->orderId_, order1.orderId_);
}

TEST_F(OrderQueueTest, PopSimple) {
  BasicOrder order1 = {101, 10, 1};
  BasicOrder order2 = {102, 20, 2};
  getQueue().push(order1);
  getQueue().push(order2);

  ASSERT_EQ(getQueue().front()->orderId_, order1.orderId_);
  getQueue().pop();

  EXPECT_EQ(getQueue().size(), 1);
  ASSERT_NE(getQueue().front(), nullptr);
  EXPECT_EQ(getQueue().front()->orderId_, order2.orderId_);

  getQueue().pop();
  EXPECT_TRUE(getQueue().empty());
}

TEST_F(OrderQueueTest, RemoveFromMiddle) {
  getQueue().push({101, 10, 1});
  Handle handle2 = getQueue().push({102, 20, 2});
  getQueue().push({103, 30, 3});

  ASSERT_EQ(getQueue().size(), 3);
  getQueue().remove(handle2);

  EXPECT_EQ(getQueue().size(), 2);
  EXPECT_EQ(getQueue().front()->orderId_, 101);
  EXPECT_EQ(getQueue().last()->orderId_, 103);
  
  getQueue().pop();
  EXPECT_EQ(getQueue().front()->orderId_, 103);
}

TEST_F(OrderQueueTest, CrossesChunkBoundaryOnPush) {
  for (size_t i = 0; i < TestChunkSize; ++i) {
    getQueue().push({static_cast<OrderId>(i), 10, 1});
  }
  
  ASSERT_EQ(getQueue().size(), TestChunkSize);
  EXPECT_EQ(getQueue().last()->orderId_, TestChunkSize - 1);

  getQueue().push({static_cast<OrderId>(TestChunkSize), 10, 1});
  
  EXPECT_EQ(getQueue().size(), TestChunkSize + 1);
  EXPECT_EQ(getQueue().front()->orderId_, 0);
  EXPECT_EQ(getQueue().last()->orderId_, TestChunkSize);
}

TEST_F(OrderQueueTest, CrossesChunkBoundaryOnPop) {
  for (size_t i = 0; i < TestChunkSize + 1; ++i) {
    getQueue().push({static_cast<OrderId>(i), 10, 1});
  }

  for (size_t i = 0; i < TestChunkSize; ++i) {
    ASSERT_EQ(getQueue().front()->orderId_, i);
    getQueue().pop();
  }
  
  ASSERT_EQ(getQueue().size(), 1);
  ASSERT_NE(getQueue().front(), nullptr);
  EXPECT_EQ(getQueue().front()->orderId_, TestChunkSize);
}

TEST_F(OrderQueueTest, RemoveAndPopAcrossChunks) {
  std::vector<Handle> handles;
  for (size_t i = 0; i < TestChunkSize * 2; ++i) {
    handles.push_back(getQueue().push({static_cast<OrderId>(i), 10, 1}));
  }

  getQueue().remove(handles[1]);
  getQueue().remove(handles[TestChunkSize]);

  getQueue().pop();
  ASSERT_EQ(getQueue().front()->orderId_, 2);
  
  for (size_t i = 2; i < TestChunkSize; ++i) {
    getQueue().pop();
  }

  ASSERT_NE(getQueue().front(), nullptr);
  EXPECT_EQ(getQueue().front()->orderId_, TestChunkSize + 1);
}

TEST_F(OrderQueueTest, StressTestWithMixedOperations) {
  TestQueue::Allocator stress_pool{500};
  TestQueue q(stress_pool);
  
  std::deque<BasicOrder> model;
  std::vector<Handle> handles;

  for (int i = 0; i < 200; ++i) {
    BasicOrder order = {static_cast<OrderId>(i), static_cast<Quantity>(i * 10), static_cast<ClientId>(i)};
    handles.push_back(q.push(order));
    model.push_back(order);
  }

  for (int i = 50; i < 100; ++i) {
    q.remove(handles[i]);
  }

  std::erase_if(model, [](const BasicOrder& o){ return o.orderId_ >= 50 && o.orderId_ < 100; });
  
  for (int i = 0; i < 50; ++i) {
    q.pop();
    if (!model.empty()) {
      model.pop_front();
    }
  }

  ASSERT_EQ(q.size(), model.size());

  while (!q.empty() && !model.empty()) {
    ASSERT_NE(q.front(), nullptr);
    EXPECT_EQ(q.front()->orderId_, model.front().orderId_);
    q.pop();
    model.pop_front();
  }

  EXPECT_TRUE(q.empty());
  EXPECT_TRUE(model.empty());
}

} // namespace stockex::models