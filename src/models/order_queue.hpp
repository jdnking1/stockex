#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "basic_types.hpp"

namespace stockex::models {

using QueueSize = uint32_t;
using QueuePosition = uint32_t;

struct BasicOrder {
  OrderId orderId_{};
  Quantity qty_{};
  ClientId clientId_{};
  bool deleted_{false};

  friend bool operator==(const BasicOrder &lhs, const BasicOrder &rhs) {
    return lhs.orderId_ == rhs.orderId_ && lhs.qty_ == rhs.qty_ &&
           lhs.clientId_ == rhs.clientId_;
  }

  friend std::ostream &operator<<(std::ostream &os, const BasicOrder &order) {
    os << "Order{id: " << order.orderId_ << ", qty: " << order.qty_ << "}";
    return os;
  }
};

class OrderQueue {
private:
  void compact() noexcept {
    if (head_ < COMPACT_THRESHOLD) {
      return;
    }

    auto remaining = tail_ - head_;
    if (remaining > 0) {
      std::move(orders_.begin() + head_, orders_.begin() + tail_,
                orders_.begin());
    }

    tail_ = remaining;
    orders_.resize(tail_);

    offset_ += head_;
    head_ = 0;
  }

  void advanceHead() noexcept {
    compact();
    while (head_ < tail_ && orders_[head_].deleted_) {
      head_++;
    }
  }

public:
  constexpr explicit OrderQueue(QueueSize initialCapacity,
                                QueueSize compactThreshold = 750000)
      : COMPACT_THRESHOLD(compactThreshold) {
    orders_.reserve(initialCapacity);
  }

  auto push(BasicOrder order) noexcept -> QueuePosition {
    orders_.push_back(order);
    tail_++;
    size_++;
    return offset_ + tail_ - 1;
  }

  auto remove(QueuePosition pos) noexcept {
    if (pos < offset_)
      return;
    auto index = pos - offset_;
    if (index < tail_ && !orders_[index].deleted_) {
      orders_[index].deleted_ = true;
      size_--;
    }
  }

  auto pop() noexcept {
    advanceHead();
    if (empty())
      return;
    if (head_ < tail_) {
      orders_[head_].deleted_ = true;
      size_--;
    }
  }

  auto front() noexcept -> BasicOrder * {
    advanceHead();
    return empty() ? nullptr : &orders_[head_];
  }

  auto front() const noexcept -> const BasicOrder * {
    QueueSize current_head = head_;
    while (current_head < tail_ && orders_[current_head].deleted_) {
      current_head++;
    }
    return current_head >= tail_ ? nullptr : &orders_[current_head];
  }

  auto last() const noexcept -> const BasicOrder * {
    auto current_pos = tail_;
    while (current_pos > head_) {
      --current_pos;
      if (!orders_[current_pos].deleted_) {
        return &orders_[current_pos];
      }
    }
    return nullptr;
  }

  auto empty() const noexcept -> bool { return size_ == 0; }
  auto size() const noexcept -> size_t { return size_; }

private:
  std::vector<BasicOrder> orders_{};
  QueueSize tail_{0};
  QueueSize head_{0};
  QueueSize offset_{0};
  QueueSize size_{0};
  const QueueSize COMPACT_THRESHOLD;
};
} // namespace stockex::models
