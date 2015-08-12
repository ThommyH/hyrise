#pragma once

/*
 * XswAllocator.h
 *
 *  Created on: May 18, 2014
 *      Author: Jan Ole Vollmer
 */

// #ifdef WITH_XSW

#include "BlockAllocator.h"
#include "XswBlockManager.h"

namespace hyrise {
namespace memory {

template <typename Type>
using XswAllocator = BlockAllocator<Type, XswBlockManager>;

}  // namespace memory
}  // namespace hyrise

// #else // WITH_XSW

// #warning "Compiled without XSW, falling back to Mmap"

// #include "MmapAllocator.h"

// namespace hyrise {
// namespace memory {

// template <typename Type>
// using XswAllocator = MmapAllocator<Type>;

// }  // namespace memory
// }  // namespace hyrise

// #endif // WITH_XSW
