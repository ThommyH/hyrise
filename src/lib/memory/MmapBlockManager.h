#pragma once

/*
 * MmapBlockManager.h
 *
 *  Created on: May 18, 2014
 *      Author: Jan Ole Vollmer
 */

#include <string>

namespace hyrise {
namespace memory {

class MmapBlockManager {
public:
  static MmapBlockManager* getDefault();

public:
  MmapBlockManager();
  MmapBlockManager(const MmapBlockManager&) = delete;
  MmapBlockManager(MmapBlockManager&&) = delete;
  virtual ~MmapBlockManager();

  MmapBlockManager& operator=(const MmapBlockManager&) = delete;

  void* allocate(std::size_t numBytes);
  void deallocate(void* ptr, std::size_t numBytes);

  std::size_t capacity() const;
  std::size_t remaining() const;
  void reset();

private:
  void configureFromEnv();
  void createMmap();

private:
  std::string m_filename;
  int m_file;
  void* m_mmap;
  std::size_t m_capacity;
  std::size_t m_currentPosition;
};

} /* namespace memory */
} /* namespace hyrise */
