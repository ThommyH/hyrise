/*
 * MmapBlockManager.cpp
 *
 *  Created on: May 18, 2014
 *      Author: Jan Ole Vollmer
 */

#ifdef __APPLE__
#define O_LARGEFILE 0x0
#else
#define _LARGEFILE64_SOURCE
#endif // __APPLE__

#include "MmapBlockManager.h"

#include <helper/literals.h>
#include <helper/logging.h>

#include <log4cxx/logger.h>

#include <boost/lexical_cast.hpp>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace hyrise {
namespace memory {

namespace {

#ifdef __APPLE__
// from Mozilla: http://hg.mozilla.org/mozilla-central/file/3d846420a907/xpcom/glue/FileUtils.cpp#l61
int fallocate(int fd, off_t len) {
    fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, len};
    // Try to get a continous chunk of disk space
    int ret = fcntl(fd, F_PREALLOCATE, &store);
    if(-1 == ret) {
        // OK, perhaps we are too fragmented, allocate non-continuous
        store.fst_flags = F_ALLOCATEALL;
        ret = fcntl(fd, F_PREALLOCATE, &store);
        if (-1 == ret)
            return false;
    }
    return 0 == ftruncate(fd, len);
}
#else
int fallocate(int fd, off64_t len) {
    return posix_fallocate64(fd, 0, len);
}
#endif

const std::string k_envPrefix = "HYRISE_MMAP_";
const std::string k_envFilename = k_envPrefix + "FILE";
const std::string k_envSize = k_envPrefix + "SIZE";

template <typename Type>
bool assignEnv(const std::string& env, Type& target) {
  const char* value = std::getenv(env.c_str());
  if (value != nullptr) {
    target = boost::lexical_cast<Type>(value);
  }
  return false;
}

} // anonymous namespace

namespace {
log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("memory.MmapBlockManager"));
}

MmapBlockManager* MmapBlockManager::getDefault() {
  static MmapBlockManager defaultManager;
  return &defaultManager;
}

MmapBlockManager::MmapBlockManager()
: m_filename("/home/Thomas.Hille/tmp/hyrise-mmap")
, m_file(-1)
, m_mmap(MAP_FAILED)
, m_capacity(25_G)
, m_currentPosition(0) {
  configureFromEnv();
  createMmap();
}

MmapBlockManager::~MmapBlockManager() {
  if (m_mmap != MAP_FAILED) {
    munmap(m_mmap, m_capacity);
  }
  if (m_file != -1) {
    close(m_file);
    remove(m_filename.c_str());
  }
}

void MmapBlockManager::configureFromEnv() {
  assignEnv(k_envFilename, m_filename);
  assignEnv(k_envSize, m_capacity);
}

void MmapBlockManager::createMmap() {
  bool ok = true;

  LOG4CXX_INFO(logger, "Creating shared mapping on file: " << m_filename << ", size: " << m_capacity << " bytes");

  m_file = open(m_filename.c_str(), O_RDWR | O_CREAT | O_LARGEFILE | O_DIRECT | O_SYNC | O_NOATIME, S_IRUSR | S_IWUSR);
  ok &= m_file != -1;
  ok &= fallocate(m_file, m_capacity) == 0;
  if (!ok) {
    LOG4CXX_THROW(logger, std::runtime_error, "Failed to open file '" + m_filename + "': " + strerror(errno));
  }

  m_mmap = mmap(nullptr, m_capacity, PROT_READ | PROT_WRITE, MAP_SHARED, m_file, 0);
  ok &= m_mmap != MAP_FAILED;
  if (!ok) {
    LOG4CXX_THROW(logger, std::runtime_error, "Failed to mmap file: " << strerror(errno));
  }
}

void* MmapBlockManager::allocate(std::size_t numBytes) {
  LOG4CXX_DEBUG(logger, "Allocating " << numBytes << " bytes starting at " << m_currentPosition);
  if (numBytes > remaining()) {
    LOG4CXX_THROW(logger, std::bad_alloc, "Can't allocate " << numBytes << " bytes, only have " << remaining() << " bytes left");
    std::bad_alloc exception;
    throw exception;	  
  }

  void* ptr = static_cast<char*>(m_mmap) + m_currentPosition;
  m_currentPosition += numBytes;
  LOG4CXX_DEBUG(logger, "Remaining space: " << remaining() << " bytes");
  return ptr;
}

void MmapBlockManager::deallocate(void* ptr, std::size_t numBytes) {
  // do nothing
  LOG4CXX_DEBUG(logger, "Deallocating " << numBytes << " bytes at " << static_cast<char*>(ptr) - static_cast<char*>(m_mmap));
}

std::size_t MmapBlockManager::capacity() const {
  return m_capacity;
}

std::size_t MmapBlockManager::remaining() const {
  return m_capacity - m_currentPosition;
}

void MmapBlockManager::reset() {
  m_currentPosition = 0;
}

} /* namespace memory */
} /* namespace hyrise */
