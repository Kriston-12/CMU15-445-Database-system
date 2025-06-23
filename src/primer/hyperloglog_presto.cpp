//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hyperloglog_presto.cpp
//
// Identification: src/primer/hyperloglog_presto.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/hyperloglog_presto.h"

namespace bustub {

/** @brief Parameterized constructor. */
template <typename KeyType>
HyperLogLogPresto<KeyType>::HyperLogLogPresto(int16_t n_leading_bits) : cardinality_(0) {
  if (n_leading_bits < 0 || n_leading_bits >= 64) {
    return; // cannot have n_bits < 0
  }
  leadingBits = n_leading_bits;
  numRegisters = 1 << n_leading_bits;
  // registers.resize(0, numRegisters);

  dense_bucket_.resize(numRegisters, std::bitset<DENSE_BUCKET_SIZE>(0));
}

template<size_t N> 
auto ComputeBinary(size_t val) -> std::bitset<N> {
  return std::bitset<N>(static_cast<uint64_t>(val));
}

// 这里返回类型其实可以是uint_8, 但是这个返回值要作为参数传给ComputeBinary这个函数，如果返回uint_8，那么传参之前要
// ComputeBinary(static_cast<size_t>(returned variable))
template <typename KeyType>
auto HyperLogLogPresto<KeyType>::HyperLogLogPresto::ComputeContinuousZeros(const std::bitset<64>& bitset) -> std::size_t {
  size_t count = 0;
  for (int i = 0; i < 64 - leadingBits; ++i) {
    if (!bitset[i]) {
      count++;
    } 
    else {
      break;
    }
  }
  return count;
}

/** @brief Element is added for HLL calculation. */
template <typename KeyType>
auto HyperLogLogPresto<KeyType>::AddElem(KeyType val) -> void {
  /** @TODO(student) Implement this function! */
  size_t hash = CalculateHash(val);
   
  std::bitset<64> entireBinary = ComputeBinary<64>(hash);
  // std::cout << "hash in binary is " << entireBinary.to_string() << std::endl;

  //leadingBits to size_t integers
  size_t bucketIndex = (entireBinary >> (64 - leadingBits)).to_ullong(); // implicit conversion from uint64_t to uint8_t.
  // std::cout << "leading bits is " << leadingBits << std::endl;
  // std::cout << "bucketIdx in binary is " << std::bitset<64>(bucketIndex).to_string() << std::endl;

  size_t contiZeros = ComputeContinuousZeros(entireBinary);
  std::bitset<TOTAL_BUCKET_SIZE> contiZerosBinary = ComputeBinary<TOTAL_BUCKET_SIZE>(contiZeros);
  // std::cout << "continuous zero is " << contiZerosBinary.to_string() << std::endl;

  std::bitset<OVERFLOW_BUCKET_SIZE> overFlowBinary = std::bitset<OVERFLOW_BUCKET_SIZE>(contiZerosBinary.to_ulong() >> DENSE_BUCKET_SIZE); // First 3 bits will be part of overflow
  // std::cout << "overflow is the first 3 bits of continuous zero, it is " << overFlowBinary.to_string() << std::endl;

  // std::bitset<DENSE_BUCKET_SIZE> denseBinary = (contiZerosBinary & 0b1111); // this line is wrong, bitset cannot have bitwise ops with integers, also contiZerosBinary has different types than denseBinary, this is also wrong
  std::bitset<DENSE_BUCKET_SIZE> denseBinary = std::bitset<DENSE_BUCKET_SIZE>((contiZerosBinary & std::bitset<TOTAL_BUCKET_SIZE>((1 << DENSE_BUCKET_SIZE) - 1)).to_ulong()); // Last 4 bits will be part of dense
  // std::cout << "dense binary is the last 4 bits of continuous zero, it is " << denseBinary.to_string() << std::endl;

  // Similar to task 1, only when contiZeros is greater then the current value we update it
  if (contiZeros > (dense_bucket_[bucketIndex].to_ulong() | (overflow_bucket_[bucketIndex].to_ulong() << DENSE_BUCKET_SIZE)))
  {
    dense_bucket_[bucketIndex] = denseBinary;
    // Update overflow only if needed
    if (overFlowBinary.any()) {
      overflow_bucket_[bucketIndex] = overFlowBinary;
    } else {
      // overflow_bucket_.erase(bucketIndex);  // optional
    }
  }
  
  

  // std::cout << "-----------------------------------------------------------" << std::endl;
  // std::cout << "-----------------------------------------------------------" << std::endl;
  // std::cout << "-----------------------------------------------------------" << std::endl;
}

/** @brief Function to compute cardinality. */
template <typename T>
auto HyperLogLogPresto<T>::ComputeCardinality() -> void {
  /** @TODO(student) Implement this function! */

  if (numRegisters == 0) { // if numRegisters == 0, it means the initialization failed, leading bits (number of buckets) is not valid
    return;
  }

  double sum = 0.0;
  for (size_t i = 0; i < numRegisters; ++i) {
    uint8_t denseVal = dense_bucket_[i].to_ulong();

    // Might not have this, if not set it to 0, else set it the val
    uint8_t overflowVal = 0;
    if (overflow_bucket_.count(i)) {
      overflowVal = overflow_bucket_[i].to_ulong();
    }

    uint8_t totalVal = (overflowVal << DENSE_BUCKET_SIZE) | denseVal;

    sum += std::pow(2.0, -static_cast<int>(totalVal));
  }

  double estimate = CONSTANT * numRegisters * numRegisters / sum;

  cardinality_ = static_cast<uint64_t>(std::floor(estimate));
}

template class HyperLogLogPresto<int64_t>;
template class HyperLogLogPresto<std::string>;
}  // namespace bustub
