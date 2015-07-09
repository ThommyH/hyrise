#pragma once

/*
 * MmapAllocator.h
 *
 *  Created on: May 18, 2014
 *      Author: Jan Ole Vollmer
 */

#include "BlockAllocator.h"
#include "MmapBlockManager.h"

namespace hyrise {
namespace memory {

template <typename Type>
using MmapAllocator = BlockAllocator<Type, MmapBlockManager>;

}  // namespace memory
}  // namespace hyrise
