#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "utils.hpp"

namespace stockex::utils {

template <typename T> class memory_pool {
public:
  explicit memory_pool(std::size_t size) : free_block_count_{size} {
    memory_.resize(size);

    for (std::size_t i = 0; i < size - 1; i++) {
      memory_[i].next_free_block_ = i + 1;
    }

    ASSERT(static_cast<void *>(&(memory_[0].data_)) ==
               static_cast<void *>(&(memory_[0])),
           "T object should be first member of ObjectBlock.");
  }

  template <typename... Args> T *alloc(Args &&...args) {
    ASSERT(free_block_count_ > 0, "No free memory blocks.");
    auto memory_block = &memory_[free_block_index_];
    ASSERT(memory_block->is_free_, "Memory block is not free.");
    memory_block->is_free_ = false;
    auto result = reinterpret_cast<T *>(&memory_[free_block_index_].data_);
    result = new (result) T(std::forward<Args>(args)...);
    free_block_index_ = memory_[free_block_index_].next_free_block_;
    --free_block_count_;
    return result;
  }

  T *raw_alloc() {
    ASSERT(free_block_count_ > 0, "No free memory blocks.");
    auto *memory_block = &memory_[free_block_index_];
    ASSERT(memory_block->is_free_, "Memory block is not free.");
    memory_block->is_free_ = false;
    free_block_index_ = memory_[free_block_index_].next_free_block_;
    free_block_count_--;
    return reinterpret_cast<T *>(&memory_[free_block_index_].data_);
  }

  void free(T *ptr) {
    std::size_t block_index =
        reinterpret_cast<const memory_block *>(ptr) - &memory_[0];
    ASSERT(block_index < memory_.size(), "Invalid memory block index.");
    auto memory_block = &memory_[block_index];
    ASSERT(!memory_block->is_free_, "Memory block is already free.");
    memory_block->is_free_ = true;
    memory_block->next_free_block_ = free_block_index_;
    free_block_index_ = block_index;
    free_block_count_++;
  }

  memory_pool(const memory_pool &) = delete;
  memory_pool(memory_pool &&) = delete;

  memory_pool &operator=(const memory_pool &) = delete;
  memory_pool &operator=(const memory_pool &&) = delete;

private:
  struct memory_block {
    alignas(T) char data_[sizeof(T)];
    std::size_t next_free_block_ = std::numeric_limits<std::size_t>::max();
    bool is_free_ = true;
  };

  std::vector<memory_block> memory_;
  std::size_t free_block_count_ = 0;
  std::size_t free_block_index_ = 0;
};

} // namespace stockex::utils