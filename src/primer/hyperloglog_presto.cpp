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
HyperLogLogPresto<KeyType>::HyperLogLogPresto(int16_t n_leading_bits) : cardinality_(0), leadingBits(n_leading_bits) {
  numRegisters = 1 << n_leading_bits;
  // registers.resize(0, numRegisters);

  dense_bucket_.resize(1 << n_leading_bits, std::bitset<DENSE_BUCKET_SIZE>(0));
}

template<size_t N> 
auto ComputeBinary(size_t val) -> std::bitset<N> {
  return std::bitset<N>(static_cast<uint64_t>val);
}

// 这里返回类型其实可以是uint_8, 但是这个返回值要作为参数传给ComputeBinary这个函数，如果返回uint_8，那么传参之前要
// ComputeBinary(static_cast<size_t>(returned variable))
auto ComputeContinuousZeros(const std::bitset<64>& bitset) -> std::size_t {
  size_t count = 0;
  for (int i = 63; i >= 0; --i) {
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

  //leadingBits to size_t integers
  size_t bucketIndex = (entireBinary >> (64 - leadingBits)).to_ullong(); // implicit conversion from uint64_t to uint8_t.

  size_t contiZeros = ComputeContinuousZeros(entireBinary);
  std::bitset<TOTAL_BUCKET_SIZE> contiZerosBinary = ComputeBinary<TOTAL_BUCKET_SIZE>(contiZeros);

  std::bitset<OVERFLOW_BUCKET_SIZE> overFlowBinary = (contiZerosBinary >> DENSE_BUCKET_SIZE);
  // std::bitset<DENSE_BUCKET_SIZE> denseBinary = (contiZerosBinary & 0b1111); // the expression below is more generalized
  std::bitset<DENSE_BUCKET_SIZE> denseBinary = contiZerosBinary & std::bitset<TOTAL_BUCKET_SIZE>((1 << DENSE_BUCKET_SIZE) - 1);

  dense_bucket_[bucketIndex] = denseBinary;

  // Update overflow only if needed
  if (overFlowBinary.any()) {
    overflow_bucket_[bucketIndex] = overFlowBinary;
  } else {
    overflow_bucket_.erase(bucketIndex);  // optional
  }
}

/** @brief Function to compute cardinality. */
template <typename T>
auto HyperLogLogPresto<T>::ComputeCardinality() -> void {
  /** @TODO(student) Implement this function! */
  
}

template class HyperLogLogPresto<int64_t>;
template class HyperLogLogPresto<std::string>;
}  // namespace bustub
