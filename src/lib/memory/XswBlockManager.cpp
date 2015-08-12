/*
 * XswBlockManager.cpp
 *
 *  Created on: May 12, 2014
 *      Author: Jan Ole Vollmer
 */

// #ifdef WITH_XSW

#include "XswBlockManager.h"

#include <helper/literals.h>
#include <helper/logging.h>

#include <log4cxx/logger.h>

#include <boost/lexical_cast.hpp>
#include <sstream>

namespace hyrise {
namespace memory {

namespace {

log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("memory.XswBlockManager"));

const std::string k_envPrefix = "HYRISE_XSW_";
const std::string k_envDevice = k_envPrefix + "DEVICE";
const std::string k_envRegion = k_envPrefix + "REGION";
const std::string k_envDebug = k_envPrefix + "DEBUG";

const std::string k_envCachePrefix = k_envPrefix + "CACHE_";
const std::string k_envCacheTag = k_envCachePrefix + "TAG";
const std::string k_envCacheSize = k_envCachePrefix + "SIZE";
const std::string k_envCacheLowWater = k_envCachePrefix + "LOW_WATER";
const std::string k_envCacheEviction = k_envCachePrefix + "EVICTION";

template <typename Type>
bool assignEnv(const std::string& env, Type& target) {
  const char* value = std::getenv(env.c_str());
  if (value != nullptr) {
    target = boost::lexical_cast<Type>(value);
  }
  return false;
}

} // anonymous namespace

XswBlockManager* XswBlockManager::getDefault() {
  static XswBlockManager defaultManager;
  return &defaultManager;
}

XswBlockManager::XswBlockManager()
: m_deviceName("/dev/fioa")
, m_regionName("hyrise")
, m_deviceHandle(nullptr)
, m_regionHandle(nullptr)
, m_mmap(nullptr)
, m_currentPosition(0)
, m_minReadahead(1)
, m_maxReadahead(64)
, m_debugMode(false) {
  configureFromEnv();

  if (m_debugMode) {
    XSWDebugEnable(XSW_DEBUG_ALL, XSW_DEBUG_ON);
    XSWDebugSetOutput(const_cast<char*>("xsw_api.log"));
  }

  m_pageCache = new PageCache;

  // open device & create mapping
  createMmap();
}

void XswBlockManager::configureFromEnv() {
  assignEnv(k_envDevice, m_deviceName);
  assignEnv(k_envRegion, m_regionName);
  assignEnv(k_envDebug, m_debugMode);
}

XswBlockManager::~XswBlockManager() {
  int errCode = 0;

  // unmap region
  if (m_mmap != nullptr) {
    errCode = XSWClose(m_mmap);
    if (errCode != 0) {
      LOG4CXX_ERROR(logger, "Failed to close mmap");
    }
  }

  // close device
  if (m_deviceHandle != nullptr) {
    errCode = XSWClose(m_deviceHandle);
    if (errCode != 0) {
      LOG4CXX_ERROR(logger, "Failed to close device");
    }
  }

  delete m_pageCache;
}

XSW_BIO* XswBlockManager::mmap() {
  return m_mmap;
}

void* XswBlockManager::allocate(std::size_t numBytes) {
  LOG4CXX_DEBUG(logger, "Allocating " << numBytes << " bytes starting at " << m_currentPosition);
  if (numBytes > remaining()) {
    LOG4CXX_THROW(logger, std::bad_alloc, "Can't allocate " << numBytes << " bytes, only have " << remaining() << " bytes left");
  }

  void* ptr = static_cast<char*>(m_mmap->mmap_vaddr) + m_currentPosition;
  m_currentPosition += numBytes;
  LOG4CXX_DEBUG(logger, "Remaining space: " << remaining() << " bytes");
  return ptr;
}

void XswBlockManager::deallocate(void* ptr, std::size_t numBytes) {
  // do nothing
  LOG4CXX_DEBUG(logger, "Deallocating " << numBytes << " bytes at " << static_cast<char*>(ptr) - static_cast<char*>(m_mmap->mmap_vaddr));
}

std::size_t XswBlockManager::capacity() const {
  return m_mmap->size;
}

std::size_t XswBlockManager::remaining() const {
  return m_mmap->size - m_currentPosition;
}

void XswBlockManager::reset() {
  m_currentPosition = 0;
}

void XswBlockManager::setTemperature(void* block, std::int64_t offset, std::uint64_t length, XSW_COLOR temperature) {
  std::uint64_t blockOffset = static_cast<char*>(block) - static_cast<char*>(m_mmap->mmap_vaddr);
  int errCode = XSWMmapSetColor(m_mmap, blockOffset + offset, length, temperature);
}

void XswBlockManager::createMmap() {
  LOG4CXX_INFO(logger, "Creating mapping on device: " << m_deviceName << ", region: " << m_regionName);

  // open device
  m_deviceHandle = XSWRawOpen(const_cast<char*>(m_deviceName.c_str()));
  if (m_deviceHandle == nullptr) {
    LOG4CXX_THROW(logger, std::runtime_error, "XswBlockManager: Failed to open device " << m_deviceName);
  }

  // open region
  m_regionHandle = XSWContainerOpenRegion(m_deviceHandle, const_cast<char*>(m_regionName.c_str()));
  if (m_regionHandle == nullptr) {
    LOG4CXX_THROW(logger, std::runtime_error, "XswBlockManager: Failed to open region " << m_regionName << " on device " << m_deviceName);
  }

  // create mapping
  // m_mmap = XSWMmapOpen(0, m_regionHandle, const_cast<char*>(m_pageCache->tag().c_str()), const_cast<char*>("xsw_memory_policy"), XSW_MMAP_SHARED);
  m_mmap = XSWMmapOpen(0, m_pageCache->size, m_regionHandle, 0, m_pageCache->pcid, XSW_MMAP_FILE|XSW_MMAP_SHARED);
  if (m_mmap == nullptr) {
    LOG4CXX_THROW(logger, std::runtime_error, "XswBlockManager: Failed to open mmap");
  }
  LOG4CXX_INFO(logger, "Size: " << m_mmap->size << " bytes");

  // enable zerofill
  XSWMmapAdvise(m_mmap, 0, m_mmap->size, XSW_ADVISE_ZEROFILL);

  // set up readahead, if configured
  LOG4CXX_INFO(logger, "Readahead: " << m_minReadahead << " - " << m_maxReadahead);
  if (m_minReadahead >= 0 && m_maxReadahead >= 0) {
    int errCode = XSWMmapSetupReadAhead(m_mmap, m_minReadahead, m_maxReadahead);
    if (errCode != 0) {
      LOG4CXX_THROW(logger, std::runtime_error, "XswBlockManager: Failed to setup readahead");
    }
    errCode = XSWMmapAdvise(m_mmap, 0, m_mmap->size, XSW_ADVISE_SEQUENTIAL);
    if (errCode != 0) {
      LOG4CXX_THROW(logger, std::runtime_error, "XswBlockManager: Failed to setup readahead");
    }
  }
}

XswBlockManager::PageCache::PageCache()
: m_tag("hyrise")
, size(8_G)
, m_lowWaterLevel(1_G)
, m_evictionSize(1_M)
, m_type(k_lru) {
  configureFromEnv();
  createPageCache();
}

XswBlockManager::PageCache::~PageCache() {
  deletePageCache();
}

void XswBlockManager::PageCache::configureFromEnv() {
  assignEnv(k_envCacheTag, m_tag);
  assignEnv(k_envCacheSize, size);
  assignEnv(k_envCacheLowWater, m_lowWaterLevel);
  assignEnv(k_envCacheEviction, m_evictionSize);
}

void XswBlockManager::PageCache::createPageCache() {
  

  // delete existing cache, if any (e.g. from a crashed previous execution)
  // if (XSWMmapQueryPageCache(const_cast<char*>(m_tag.c_str()), nullptr) == 0) {
  //   LOG4CXX_WARN(logger, "page cache '" << m_tag << "' already exists, deleting it...");
  //   deletePageCache();
  // }

  LOG4CXX_INFO(logger, "Creating page cache '" << m_tag << "' with size: " << size << ", low water level: " << m_lowWaterLevel << ", eviction size: " << m_evictionSize << ", type: " << m_type);

  // create cache
  //errCode = XSWMmapCreatePageCache(const_cast<char*>(m_tag.c_str()), size, m_lowWaterLevel, m_evictionSize, m_type);
  pcid = XSWMmapCreatePageCache(const_cast<char*>(m_tag.c_str()), size, m_lowWaterLevel, m_evictionSize, m_type);
  if (pcid == 0) {
    LOG4CXX_THROW(logger, std::runtime_error, "XswBlockManager: Failed to create page cache");
  }
}

void XswBlockManager::PageCache::deletePageCache() {
  LOG4CXX_INFO(logger, "Deleting page cache '" << m_tag << "'");

  // int errCode = XSWMmapDeletePageCache(const_cast<char*>(m_tag.c_str()));
  int errCode = XSWMmapDeletePageCache(pcid);
  if (errCode != 0) {
    LOG4CXX_WARN(logger, "Failed to delete page cache '" << m_tag << "'");
  }
}

}  // namespace memory
}  // namespace hyrise

// #endif // WITH_XSW
