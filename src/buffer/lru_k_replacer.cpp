//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include "common/exception.h"
// #include <iostream>

namespace bustub {

/**
 *
 * TODO(P1): Add implementation
 *
 * @brief a new LRUKReplacer.
 * @param num_frames the maximum number of frames the LRUReplacer will be required to store
 */
LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {
    // frames.resize(num_frames);  // std::vector.resize() 会调用default constructor，但是LRUKNode没有default constructor，会报错

    // 下面这样做可以绕开default constructor，这里相当于对frames中的每个vector调用LRUKNode(k)--emplace_back(k) 构造出 LRUKNode(k)
    frames.reserve(num_frames);
    for (size_t i = 0; i < num_frames; ++i) {
        frames.emplace_back(k);
    }
    std::cout << "-----------------initialization------------------" << std::endl;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Find the frame with largest backward k-distance and evict that frame. Only frames
 * that are marked as 'evictable' are candidates for eviction.
 *
 * A frame with less than k historical references is given +inf as its backward k-distance.
 * If multiple frames have inf backward k-distance, then evict frame whose oldest timestamp
 * is furthest in the past.
 *
 * Successful eviction of a frame should decrement the size of replacer and remove the frame's
 * access history.
 *
 * @return the frame ID if a frame is successfully evicted, or `std::nullopt` if no frames can be evicted.
 */
auto LRUKReplacer::Evict() -> std::optional<frame_id_t> { 
    frame_id_t victim = -1;
    size_t earliestKInf = std::numeric_limits<size_t>::max(); // This is for under-k distance
    size_t maxDist = std::numeric_limits<size_t>::max();  // This is for frames with K access
    bool underK = true;

    for (size_t i = 0; i < replacer_size_; ++i) {
        auto &frame = frames[i];
        if (!frame.isEvictable || frame.isRemoved) {continue;}
        std::scoped_lock lock(latch_);
        if (!frame.alreadyHasKAccess()) {
            if (frame.getEarliestTime() < earliestKInf) {
                earliestKInf = frame.getEarliestTime();
                victim = i;
            }
            underK = false;
        }
        else if (underK) {
            if (frame.getEarliestTime() < maxDist) {
                maxDist = frame.getEarliestTime();
                victim = i;
            }
        }
    }

    if (victim == -1) {
        return std::nullopt;
    }

    // node_store_.erase(victim);
    frames[victim].reset();
    curr_size_--;
    return victim;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Record the event that the given frame id is accessed at current timestamp.
 * Create a new entry for access history if frame id has not been seen before.
 *
 * If frame id is invalid (ie. larger than replacer_size_), throw an exception. You can
 * also use BUSTUB_ASSERT to abort the process if frame id is invalid.
 *
 * @param frame_id id of frame that received a new access.
 * @param access_type type of access that was received. This parameter is only needed for
 * leaderboard tests.
 */
void LRUKReplacer::RecordAccess(frame_id_t frame_id, AccessType access_type) {
    // LRUKNode& frame = node_store_[frame_id];
    LRUKNode& frame = frames[frame_id];
    std::scoped_lock lock(latch_);
    frame.frameRecordAccess(current_timestamp_++);
    // curr_size_++;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Toggle whether a frame is evictable or non-evictable. This function also
 * controls replacer's size. Note that size is equal to number of evictable entries.
 *
 * If a frame was previously evictable and is to be set to non-evictable, then size should
 * decrement. If a frame was previously non-evictable and is to be set to evictable,
 * then size should increment.
 *
 * If frame id is invalid, throw an exception or abort the process.
 *
 * For other scenarios, this function should terminate without modifying anything.
 *
 * @param frame_id id of frame whose 'evictable' status will be modified
 * @param set_evictable whether the given frame is evictable or not
 */
void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
    if (frame_id >= static_cast<int>(replacer_size_)) {
        return;
    }


    // LRUKNode &frame = node_store_[frame_id];
    LRUKNode& frame = frames[frame_id];
    std::scoped_lock lock(latch_);
    if (frame.isEvictable != set_evictable) {
        frame.isEvictable = set_evictable;
        curr_size_ += set_evictable ? 1 : -1;
        // std::cout << "curr_size is " << std::endl;
    }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Remove an evictable frame from replacer, along with its access history.
 * This function should also decrement replacer's size if removal is successful.
 *
 * Note that this is different from evicting a frame, which always remove the frame
 * with largest backward k-distance. This function removes specified frame id,
 * no matter what its backward k-distance is.
 *
 * If Remove is called on a non-evictable frame, throw an exception or abort the
 * process.
 *
 * If specified frame is not found, directly return from this function.
 *
 * @param frame_id id of frame to be removed
 */
void LRUKReplacer::Remove(frame_id_t frame_id) {
    if (frames[frame_id].isRemoved) {
        return;
    }
    // LRUKNode& frame = node_store_[frame_id];
    LRUKNode& frame = frames[frame_id];
    if (!frame.isEvictable) {
        return;
    }
    // node_store_.erase(frame_id);  // Just clear the history, will reuse the frame in frames
    std::scoped_lock lock(latch_);
    frame.reset();
    curr_size_--;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Return replacer's size, which tracks the number of evictable frames.
 *
 * @return size_t
 */
auto LRUKReplacer::Size() -> size_t { return curr_size_; }

}  // namespace bustub
