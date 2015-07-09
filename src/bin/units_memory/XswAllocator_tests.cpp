/*
 * XswAllocator_tests.cpp
 *
 *  Created on: May 11, 2014
 *      Author: Jan Ole Vollmer
 */

#ifdef WITH_XSW

#include "testing/test.h"

#include "helper/literals.h"

#include "memory/XswAllocator.h"

namespace hyrise {
namespace memory {

class XswAllocatorTests : public Test {
 public:
  virtual void SetUp() {
    XswBlockManager::getDefault()->reset();
  }
};

TEST_F(XswAllocatorTests, Allocate) {
  XswAllocator<long> allocator;
  long* ptr = allocator.allocate(1000);
  for (int i = 0; i < 1000; ++i) {
    ptr[i] = i;
  }
  allocator.deallocate(ptr, 1000);
}

TEST_F(XswAllocatorTests, BadAlloc) {
  XswAllocator<long> allocator;
  EXPECT_THROW(allocator.allocate(1000_T), std::bad_alloc);
}

TEST_F(XswAllocatorTests, Vector) {
	std::vector<long, XswAllocator<long>> vector;
	vector.resize(1000);
	std::fill(vector.begin(), vector.end(), 1);
	vector.reserve(2000);
	vector.shrink_to_fit();
}

TEST_F(XswAllocatorTests, DISABLED_SequentialAccess) { // might be slow
  XswAllocator<std::uint64_t> allocator;
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

#endif // WITH_XSW
