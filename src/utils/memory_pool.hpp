#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "utils.hpp"

namespace stockex::utils {

template <typename T> class MemoryPool {
public:
  explicit MemoryPool(std::size_t size) : freeBlockCount_{size} {
    memory_.resize(size);

    for (std::size_t i = 0; i < size - 1; i++) {
      memory_[i].next_ = i + 1;
    }

    ASSERT(static_cast<void *>(&(memory_[0].data_)) ==
               static_cast<void *>(&(memory_[0])),
           "T object should be first member of ObjectBlock.");
  }

  template <typename... Args> T *alloc(Args &&...args) {
    ASSERT(freeBlockCount_ > 0, "No free memory blocks.");
    auto memory_block = &memory_[freeBlockIndex_];
#ifndef NDEBUG
    ASSERT(memory_block->isFree_, "Memory block is not free.");
    memory_block->isFree_ = false;
#endif
    auto result = reinterpret_cast<T *>(&memory_block->data_);
    result = new (result) T(std::forward<Args>(args)...);
    freeBlockIndex_ = memory_[freeBlockIndex_].next_;
    --freeBlockCount_;
    return result;
  }

  T *rawAlloc() {
    ASSERT(freeBlockCount_ > 0, "No free memory blocks.");
    auto *memory_block = &memory_[freeBlockIndex_];
#ifndef NDEBUG
    ASSERT(memory_block->isFree_, "Memory block is not free.");
    memory_block->isFree_ = false;
#endif
    std::size_t current_index = freeBlockIndex_;
    freeBlockIndex_ = memory_block->next_;
    freeBlockCount_--;
    return reinterpret_cast<T *>(&memory_[current_index].data_);
  }

  void free(T *ptr) {
    std::size_t block_index =
        reinterpret_cast<const MemoryBlock *>(ptr) - &memory_[0];
    ASSERT(block_index < memory_.size(), "Invalid memory block index.");
    auto memory_block = &memory_[block_index];
#ifndef NDEBUG
    ASSERT(!memory_block->isFree_, "Memory block is already free.");
    memory_block->isFree_ = true;
#endif
    memory_block->next_ = freeBlockIndex_;
    freeBlockIndex_ = block_index;
    freeBlockCount_++;
  }

  MemoryPool(const MemoryPool &) = delete;
  MemoryPool(MemoryPool &&) = delete;

  MemoryPool &operator=(const MemoryPool &) = delete;
  MemoryPool &operator=(const MemoryPool &&) = delete;

private:
  struct MemoryBlock {
    alignas(T) char data_[sizeof(T)];
    std::size_t next_{std::numeric_limits<std::size_t>::max()};
#ifndef NDEBUG
    bool isFree_{true};
#endif
  };

  std::vector<MemoryBlock> memory_;
  std::size_t freeBlockCount_{};
  std::size_t freeBlockIndex_{};
};

} // namespace stockex::utils