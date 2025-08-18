#pragma once

#include <array>
#include <bit>
#include <immintrin.h>

#include "basic_types.hpp"
#include "models/constants.hpp"
#include "utils/memory_pool.hpp"

namespace stockex::models {

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
  static constexpr std::size_t BitsPerWord = 64;
  static constexpr std::size_t NumBitmapWords =
      (ChunkSize + BitsPerWord - 1) / BitsPerWord;

  struct Chunk {
    std::array<BasicOrder, ChunkSize> orders{};
    std::array<std::uint64_t, NumBitmapWords> validityBitmap{};
    std::size_t highWaterMark{};
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
    if (tailChunk_->highWaterMark >= ChunkSize) {
      allocateNewChunk();
    }
    const auto index = tailChunk_->highWaterMark;
    tailChunk_->orders[index] = order;
    const auto wordIndex = index / BitsPerWord;
    const auto bitIndex = index % BitsPerWord;
    tailChunk_->validityBitmap[wordIndex] |= (1ULL << bitIndex);
    tailChunk_->highWaterMark++;
    totalSize_++;
    return Handle{tailChunk_, index};
  }

  auto remove(Handle handle) noexcept -> void {
    if (handle.chunk_) {
      const auto wordIndex = handle.index_ / BitsPerWord;
      const auto bitIndex = handle.index_ % BitsPerWord;
      const std::uint64_t mask = (1ULL << bitIndex);

      if (handle.chunk_->validityBitmap[wordIndex] & mask) {
        handle.chunk_->validityBitmap[wordIndex] &= ~mask;
        totalSize_--;
      }
    }
  }

  auto pop() noexcept -> void {
    advanceHead();
    remove({headChunk_, headOrderIndex_});
  }

  [[nodiscard]] auto front() noexcept -> BasicOrder * {
    advanceHead();
    return &headChunk_->orders[headOrderIndex_];
  }

  [[nodiscard]] auto front() const noexcept -> const BasicOrder * {
    if (empty()) {
      return nullptr;
    }

    const auto *currentChunk = headChunk_;
    auto currentIndex = headOrderIndex_;

    while (currentChunk) {

      if (auto wordIndex = currentIndex / BitsPerWord;
          wordIndex < NumBitmapWords) {
        auto word = currentChunk->validityBitmap[wordIndex];
        word &= ~((1ULL << (currentIndex % BitsPerWord)) - 1);

        while (wordIndex < NumBitmapWords) {
          if (word != 0) {
            const auto nextBitOffset = _tzcnt_u64(word);
            const auto foundIndex = wordIndex * BitsPerWord + nextBitOffset;
            if (foundIndex < currentChunk->highWaterMark) {
              return &currentChunk->orders[foundIndex];
            }
          }
          wordIndex++;
          if (wordIndex < NumBitmapWords) {
            word = currentChunk->validityBitmap[wordIndex];
          }
        }
      }
      currentChunk = currentChunk->next;
      currentIndex = 0;
    }

    return nullptr;
  }

  [[nodiscard]] auto last() const noexcept -> const BasicOrder * {
    if (empty()) {
      return nullptr;
    }

    const auto *currentChunk = tailChunk_;
    auto currentIndex = static_cast<long>(currentChunk->highWaterMark) - 1;

    while (currentChunk) {
      if (currentIndex < 0) {
        currentChunk = currentChunk->prev;
        if (currentChunk) {
          currentIndex = static_cast<long>(currentChunk->highWaterMark) - 1;
        }
        continue;
      }

      long wordIndex = currentIndex / BitsPerWord;

      while (wordIndex >= 0) {

        if (std::uint64_t word = currentChunk->validityBitmap[wordIndex];
            word != 0) {
          const auto lastBitOffset = (BitsPerWord - 1) - _lzcnt_u64(word);
          const auto foundIndex = wordIndex * BitsPerWord + lastBitOffset;
          return &currentChunk->orders[foundIndex];
        }
        wordIndex--;
        currentIndex = wordIndex * BitsPerWord + (BitsPerWord - 1);
      }

      currentChunk = currentChunk->prev;
      if (currentChunk) {
        currentIndex = static_cast<long>(currentChunk->highWaterMark) - 1;
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
    while (headChunk_ != nullptr) {
      auto &bitmap = headChunk_->validityBitmap;
      auto wordIndex = headOrderIndex_ / BitsPerWord;

      constexpr size_t WordsPerSimdReg = 4;
      while (wordIndex + WordsPerSimdReg <= NumBitmapWords) {
        __m256i bitmapSegment = _mm256_loadu_si256(
            reinterpret_cast<const __m256i *>(&bitmap[wordIndex]));
        if (_mm256_testz_si256(bitmapSegment, bitmapSegment) == 0) {
          break;
        }
        wordIndex += WordsPerSimdReg;
      }

      while (wordIndex < NumBitmapWords && bitmap[wordIndex] == 0) {
        wordIndex++;
      }

      if (wordIndex < NumBitmapWords) {
        const auto nextBitOffset = std::countr_zero(bitmap[wordIndex]);
        headOrderIndex_ = wordIndex * BitsPerWord + nextBitOffset;

        if (headOrderIndex_ < headChunk_->highWaterMark) {
          return;
        }
      }

      Chunk *oldHead = headChunk_;
      headChunk_ = headChunk_->next;
      allocator_.free(oldHead);
      if (headChunk_ != nullptr) {
        headChunk_->prev = nullptr;
        headOrderIndex_ = 0;
      } else {
        tailChunk_ = nullptr;
        headOrderIndex_ = 0;
      }
    }
  }

  Allocator &allocator_{};
  Chunk *headChunk_{};
  Chunk *tailChunk_{};
  std::size_t headOrderIndex_{};
  std::size_t totalSize_{};
};
} // namespace stockex::models