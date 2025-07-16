#pragma once

#include <array>
#include <cstdint>
#include <immintrin.h> // For _tzcnt_u64

#include "basic_types.hpp"
#include "models/constants.hpp"
#include "utils/memory_pool.hpp"

namespace stockex::models {

// The 'deleted_' flag is no longer needed.
struct BasicOrder {
  OrderId orderId_{};
  Quantity qty_{};
  ClientId clientId_{};
};

template <std::size_t ChunkSize = QUEUE_CHUNK_SIZE> class OrderQueue;

template <std::size_t ChunkSize> struct OrderHandle {
  typename OrderQueue<ChunkSize>::Chunk *chunk_{};
  std::size_t index_{};
};

template <std::size_t ChunkSize> class OrderQueue {
public:
  static constexpr std::size_t NumBitmapWords = (ChunkSize + 63) / 64;

  struct Chunk {
    std::array<BasicOrder, ChunkSize> orders{};
    std::array<std::uint64_t, NumBitmapWords> validityBitmap{};
    std::size_t count{}; // High-water mark of insertions
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

  // Deleted copy/move constructors and assignments
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

    // Set the corresponding bit in the validity bitmap
    const std::size_t wordIndex = index / 64;
    const std::size_t bitIndex = index % 64;
    tailChunk_->validityBitmap[wordIndex] |= (1ULL << bitIndex);

    tailChunk_->count++;
    totalSize_++;

    return Handle{tailChunk_, index};
  }

  auto remove(Handle handle) noexcept -> void {
    if (handle.chunk_) {
      const std::size_t wordIndex = handle.index_ / 64;
      const std::size_t bitIndex = handle.index_ % 64;
      const std::uint64_t mask = (1ULL << bitIndex);

      // Check if the bit is set before clearing it to avoid double-deletes
      if (handle.chunk_->validityBitmap[wordIndex] & mask) {
        handle.chunk_->validityBitmap[wordIndex] &= ~mask; // Clear the bit
        totalSize_--;
      }
    }
  }

  auto pop() noexcept -> void {
    advanceHead();
    if (empty())
      return;
    remove({headChunk_, headOrderIndex_});
  }

  [[nodiscard]] auto front() noexcept -> BasicOrder * {
    advanceHead();
    return empty() ? nullptr : &headChunk_->orders[headOrderIndex_];
  }

  [[nodiscard]] auto front() const noexcept -> const BasicOrder * {
    if (empty()) {
      return nullptr;
    }

    const Chunk *currentChunk = headChunk_;
    std::size_t currentIndex = headOrderIndex_;

    while (currentChunk) {
      std::size_t wordIndex = currentIndex / 64;

      if (wordIndex < NumBitmapWords) {
        std::uint64_t word = currentChunk->validityBitmap[wordIndex];
        // Mask off bits we've already processed
        word &= ~((1ULL << (currentIndex % 64)) - 1);

        while (wordIndex < NumBitmapWords) {
          if (word != 0) {
            const auto nextBitOffset = _tzcnt_u64(word);
            const auto foundIndex = wordIndex * 64 + nextBitOffset;
            if (foundIndex < currentChunk->count) {
              return &currentChunk->orders[foundIndex];
            }
          }
          // Move to the next word
          wordIndex++;
          if (wordIndex < NumBitmapWords) {
            word = currentChunk->validityBitmap[wordIndex];
          }
        }
      }

      // No valid orders in the rest of this chunk, move to the next one
      currentChunk = currentChunk->next;
      currentIndex = 0; // Start search from the beginning of the next chunk
    }

    return nullptr; // Should not be reached if totalSize_ is accurate
  }

  [[nodiscard]] auto last() const noexcept -> const BasicOrder * {
    if (empty()) {
      return nullptr;
    }

    const Chunk *currentChunk = tailChunk_;
    long currentIndex = static_cast<long>(currentChunk->count) - 1;

    while (currentChunk) {
      if (currentIndex < 0) { // If the current chunk is empty
        currentChunk = currentChunk->prev;
        if (currentChunk) {
          currentIndex = static_cast<long>(currentChunk->count) - 1;
        }
        continue;
      }

      long wordIndex = currentIndex / 64;

      while (wordIndex >= 0) {
        std::uint64_t word = currentChunk->validityBitmap[wordIndex];
        // Mask off bits after the current index
        word &= (1ULL << ((currentIndex % 64) + 1)) - 1;

        if (word != 0) {
          // 63 - _lzcnt finds the index of the most significant (left-most) set
          // bit
          const auto lastBitOffset = 63 - _lzcnt_u64(word);
          const auto foundIndex = wordIndex * 64 + lastBitOffset;
          return &currentChunk->orders[foundIndex];
        }
        // Move to the previous word
        wordIndex--;
        currentIndex = wordIndex * 64 + 63;
      }

      // No valid orders found in this chunk, move to the previous one
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
    newChunk->count = 0;
    for (auto &word : newChunk->validityBitmap) {
      word = 0;
    } // Ensure bitmap is zeroed

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
      std::size_t wordIndex = headOrderIndex_ / 64;
      std::uint64_t currentWord = headChunk_->validityBitmap[wordIndex];

      // Mask off bits we've already processed in the current word
      currentWord &= ~((1ULL << (headOrderIndex_ % 64)) - 1);

      while (wordIndex < NumBitmapWords) {
        if (currentWord != 0) {
          // Found a valid order in this word
          const auto nextBitOffset = _tzcnt_u64(currentWord);
          headOrderIndex_ = wordIndex * 64 + nextBitOffset;

          // Ensure we haven't advanced past the number of inserted items
          if (headOrderIndex_ < headChunk_->count) {
            return; // Found the next valid order
          }
        }
        // Move to the next word in the bitmap
        wordIndex++;
        if (wordIndex < NumBitmapWords) {
          currentWord = headChunk_->validityBitmap[wordIndex];
        }
      }

      // No valid orders left in this chunk, move to the next one.
      if (headChunk_ == tailChunk_) { // Last chunk
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