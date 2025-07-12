#pragma once

#include <array>

#include "basic_types.hpp"
#include "models/constants.hpp"
#include "utils/memory_pool.hpp"

namespace stockex::models {

struct BasicOrder {
  OrderId orderId_{};
  Quantity qty_{};
  ClientId clientId_{};
  bool deleted_{false};
};

template <std::size_t ChunkSize = QUEUE_CHUNK_SIZE> class OrderQueue;

template <std::size_t ChunkSize> struct OrderHandle {
  typename OrderQueue<ChunkSize>::Chunk *chunk_{};
  std::size_t index_{};
};

template <std::size_t ChunkSize> class OrderQueue {
public:
  struct Chunk {
    std::array<BasicOrder, ChunkSize> orders{};
    std::size_t count{};
    Chunk *next{};
    Chunk *prev{};
  };

  using Handle = OrderHandle<ChunkSize>;
  using Allocator = utils::MemoryPool<Chunk>;

  explicit OrderQueue(Allocator &allocator) : allocator_{allocator} {
    allocateNewChunk();
  }

  ~OrderQueue() {
    Chunk *current = headChunk_;
    while (current != nullptr) {
      Chunk *next = current->next;
      allocator_.free(current);
      current = next;
    }
  }

  OrderQueue(const OrderQueue &) = delete;
  OrderQueue &operator=(const OrderQueue &) = delete;
  OrderQueue(OrderQueue &&) = delete;
  OrderQueue &operator=(OrderQueue &&) = delete;

  auto push(BasicOrder order) -> Handle {
    if (tailChunk_->count >= ChunkSize) {
      allocateNewChunk();
    }

    const std::size_t index = tailChunk_->count;
    tailChunk_->orders[index] = order;
    tailChunk_->count++;
    totalSize_++;

    return Handle{tailChunk_, index};
  }

  auto remove(Handle handle) noexcept -> void {
    if (handle.chunk_ && !handle.chunk_->orders[handle.index_].deleted_) {
      handle.chunk_->orders[handle.index_].deleted_ = true;
      totalSize_--;
    }
  }

  auto pop() noexcept -> void {
    advanceHead();
    if (empty())
      return;
    headChunk_->orders[headOrderIndex_].deleted_ = true;
    totalSize_--;
  }

  [[nodiscard]] auto front() noexcept -> BasicOrder * {
    advanceHead();
    return empty() ? nullptr : &headChunk_->orders[headOrderIndex_];
  }

  [[nodiscard]] auto front() const noexcept -> const BasicOrder * {
    if (empty()) {
      return nullptr;
    }
    const auto *currentChunk = headChunk_;
    auto currentIndex = headOrderIndex_;
    while (currentChunk) {
      while (currentIndex < currentChunk->count) {
        if (!currentChunk->orders[currentIndex].deleted_) {
          return &currentChunk->orders[currentIndex];
        }
        currentIndex++;
      }
      currentChunk = currentChunk->next;
      currentIndex = 0;
    }
    return nullptr;
  }

  [[nodiscard]] auto last() noexcept -> BasicOrder * {
    return const_cast<BasicOrder *>(
        static_cast<const OrderQueue *>(this)->last());
  }

  [[nodiscard]] auto last() const noexcept -> const BasicOrder * {
    if (empty()) {
      return nullptr;
    }
    const Chunk *currentChunk = tailChunk_;
    auto currentIndex = static_cast<long>(currentChunk->count) - 1;

    while (currentChunk) {
      while (currentIndex >= 0) {
        if (!currentChunk->orders[currentIndex].deleted_) {
          return &currentChunk->orders[currentIndex];
        }
        currentIndex--;
      }
      currentChunk = currentChunk->prev;
      if (currentChunk) {
        currentIndex = static_cast<long>(currentChunk->count) - 1;
      }
    }
    return nullptr;
  }

  [[nodiscard]] auto empty() const noexcept -> bool { return totalSize_ == 0; }
  [[nodiscard]] auto size() const noexcept -> std::size_t { return totalSize_; }

private:
  auto allocateNewChunk() noexcept -> void {
    auto *newChunk = allocator_.alloc();
    if (headChunk_ == nullptr) {
      headChunk_ = tailChunk_ = newChunk;
    } else {
      tailChunk_->next = newChunk;
      newChunk->prev = tailChunk_;
      tailChunk_ = newChunk;
    }
  }

  auto advanceHead() noexcept -> void {
    if (empty())
      return;

    while (headChunk_ != nullptr) {
      while (headOrderIndex_ < headChunk_->count) {
        if (!headChunk_->orders[headOrderIndex_].deleted_) {
          return;
        }
        headOrderIndex_++;
      }

      if (headChunk_ == tailChunk_) {
        allocator_.free(headChunk_);
        headChunk_ = nullptr;
        tailChunk_ = nullptr;
        headOrderIndex_ = 0;
        return;
      }

      auto *oldHead = headChunk_;
      headChunk_ = headChunk_->next;
      headOrderIndex_ = 0;
      allocator_.free(oldHead);
    }
  }

  Allocator &allocator_{};
  Chunk *headChunk_{};
  Chunk *tailChunk_{};
  std::size_t headOrderIndex_{};
  std::size_t totalSize_{};
};
} // namespace stockex::models