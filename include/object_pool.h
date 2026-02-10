#pragma once

#include <vector>
#include <cstddef>  // for size_t
#include <cassert>

template <typename T>
class ObjectPool {
public:
    // Constructor to initialize the pool with a specified size
    explicit ObjectPool(size_t pool_size) {
        pool_.reserve(pool_size);
        for (size_t i = 0; i < pool_size; ++i) {
            pool_.emplace_back();
            free_indices_.push_back(i);
        }
    }

    /**
     * Allocate an object from the pool.
     * @return Pointer to allocated object, or nullptr if pool is exhausted.
     */
    T* allocate() {
        if (free_indices_.empty()) {
            return nullptr;  // Pool exhausted
        }
        size_t index = free_indices_.back();
        free_indices_.pop_back();
        return &pool_[index];
    }

    /**
     * Return an object to the pool.
     * @param obj Pointer to object to deallocate.
     */
    void deallocate(T* obj) {
        size_t index = obj - &pool_[0];
        assert(index < pool_.size());  // Ensure the object belongs to the pool
        free_indices_.push_back(index);
    }

    /**
     * Get the number of available slots in the pool.
     * @return Number of free objects.
     */
    size_t available() const {
        return free_indices_.size();
    }

    /**
     * Get the total capacity of the pool.
     * @return Total pool capacity.
     */
    size_t capacity() const {
        return pool_.size();
    }

private:
    std::vector<T> pool_;              // Pre-allocated objects
    std::vector<size_t> free_indices_; // Stack of free indices
};  
