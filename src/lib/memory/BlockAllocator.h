#pragma once

/*
 * BlockAllocator.h
 *
 *  Created on: May 11, 2014
 *      Author: Jan Ole Vollmer
 */

namespace hyrise {
namespace memory {

template <typename Type, typename BlockManager>
struct BlockAllocator {
	using value_type = Type;

	template <typename Other>
	struct rebind {
	  using other = BlockAllocator<Other, BlockManager>;
	};

	Type* allocate(std::size_t n);
	void deallocate(Type* ptr, std::size_t n);

	std::size_t capacity() const;
	std::size_t remaining() const;
};

template <typename Type, typename BlockManager>
Type* BlockAllocator<Type, BlockManager>::allocate(std::size_t n) {
  return static_cast<Type*>(BlockManager::getDefault()->allocate(n * sizeof(Type)));
}

template <typename Type, typename BlockManager>
void BlockAllocator<Type, BlockManager>::deallocate(Type* ptr, std::size_t n) {
  return BlockManager::getDefault()->deallocate(ptr, n * sizeof(Type));
}

template <typename Type, typename BlockManager>
std::size_t BlockAllocator<Type, BlockManager>::capacity() const {
  return BlockManager::getDefault()->capacity() / sizeof(Type);
}

template <typename Type, typename BlockManager>
std::size_t BlockAllocator<Type, BlockManager>::remaining() const {
  return BlockManager::getDefault()->remaining() / sizeof(Type);
}

} /* namespace memory */
} /* namespace hyrise */
