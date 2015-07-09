/*
 * MmapAllocator_tests.cpp
 *
 *  Created on: May 11, 2014
 *      Author: Jan Ole Vollmer
 */

#include "testing/test.h"

#include "helper/literals.h"

#include "memory/MmapAllocator.h"

namespace hyrise {
namespace memory {

class MmapAllocatorTests : public Test {
 public:
  virtual void SetUp() {
    MmapBlockManager::getDefault()->reset();
  }
};

TEST_F(MmapAllocatorTests, Allocate) {
  MmapAllocator<long> allocator;
  long* ptr = allocator.allocate(1000);
  for (int i = 0; i < 1000; ++i) {
    ptr[i] = i;
  }
  allocator.deallocate(ptr, 1000);
}

TEST_F(MmapAllocatorTests, BadAlloc) {
  MmapAllocator<long> allocator;
  EXPECT_THROW(allocator.allocate(1000_T), std::bad_alloc);
}

TEST_F(MmapAllocatorTests, Vector) {
	std::vector<long, MmapAllocator<long>> vector;
	vector.resize(1000);
	std::fill(vector.begin(), vector.end(), 1);
	vector.reserve(2000);
	vector.shrink_to_fit();
}

TEST_F(MmapAllocatorTests, DISABLED_SequentialAccess) { // might be slow
  MmapAllocator<std::uint64_t> allocator;
  std::size_t size = allocator.remaining();
  std::uint64_t* pointer = allocator.allocate(size);

  #pragma omp parallel for num_threads(4)
  for (std::size_t i = 0; i < size; ++i) {
    pointer[i] = 0xFFFFFFFFFFFFFFFF;
  }

  std::uint64_t tmp = 0;
  for (std::size_t i = 0; i < size; ++i) {
    tmp |= pointer[i];
  }
  EXPECT_EQ(0xFFFFFFFFFFFFFFFF, tmp);
}

}  // namespace memory
}  // namespace hyrise

